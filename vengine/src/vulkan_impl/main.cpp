#include <vulkan_impl/components/device.h>
#include <Utility/BinaryReader.h>
#include <vulkan_impl/shader/compute_shader.h>
#include <vulkan_impl/shader/shader_code.h>
#include <vulkan_impl/gpu_collection/buffer.h>
#include <vulkan_impl/shader/descriptor_pool.h>
#include <vulkan_impl/shader/descriptorset_manager.h>
#include <vulkan_impl/runtime/command_pool.h>
#include <vulkan_impl/runtime/frame_resource.h>
#include <vulkan_impl/runtime/res_state_tracker.h>
#include <vulkan_impl/gpu_collection/texture.h>
#include <vulkan_impl/rtx/mesh.h>
#include <vulkan_impl/rtx/accel.h>
#include <dxc/dxc_util.h>
#include <vulkan_impl/rtx/query.h>
#include <vulkan_impl/gpu_collection/bindless_array.h>
using namespace toolhub::vk;

static void ComputeShaderTest(Device const* device, vstd::span<vbyte const> block) {
	ShaderCode shaderCode(device, block, {});

	vstd::small_vector<VkDescriptorType> types;
	types.emplace_back(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	types.emplace_back(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	ComputeShader cs(
		device,
		shaderCode,
		types,
		uint3(1, 1, 1));
	Buffer defaultBuffer(
		device,
		sizeof(float),
		false,
		RWState::None,
		0);
	Buffer outputDefault(
		device,
		sizeof(float),
		false,
		RWState::None,
		0);

	CommandPool cmdPool(device);
	FrameResource frameRes(device, &cmdPool);

	ResStateTracker stateTracker(device);
	vstd::vector<vstd::move_only_func<void()>> disposeFuncs;
	vstd::move_only_func<void(vstd::move_only_func<void()> &&)> addDisposeEvent = [&](auto&& v) { disposeFuncs.push_back(std::move(v)); };
	BufferView readback;
	BindlessArray arr(device, 256);
	arr.Bind(244, &defaultBuffer, 0);
	if (auto cmdBuffer = frameRes.AllocateCmdBuffer()) {
		auto Cut = [&] {
			stateTracker.Execute(cmdBuffer);
			device->UpdateBindless();
			frameRes.ExecuteCopy(cmdBuffer);
		};
		auto upload = frameRes.AllocateUpload(4);
		readback = frameRes.AllocateReadback(4);
		float inputFloat = 365;
		upload.buffer->CopyValueFrom(inputFloat, upload.offset);
		stateTracker.MarkBufferWrite(
			&defaultBuffer,
			BufferWriteState::Copy);
		arr.Preprocess(
			&frameRes,
			stateTracker,
			cmdBuffer);
		Cut();
		cmdBuffer->CopyBuffer(
			upload.buffer,
			upload.offset,
			&defaultBuffer,
			0,
			4);
		stateTracker.MarkBufferWrite(
			&outputDefault,
			BufferWriteState::Compute);
		vstd::vector<BindResource> binds;
		binds.emplace_back(&outputDefault, true);
		binds.emplace_back(arr.InstanceBuffer(), false);
		auto set = cmdBuffer->PreprocessDispatch(
			&cs,
			stateTracker,
			binds);
		stateTracker.MarkBindlessRead(arr);
		Cut();
		cmdBuffer->Dispatch(
			set,
			&cs,
			uint3(1, 1, 1));
		stateTracker.MarkBufferRead(
			&outputDefault,
			BufferReadState::ComputeOrCopy);
		Cut();
		cmdBuffer->CopyBuffer(
			&outputDefault,
			0,
			readback.buffer,
			readback.offset,
			4);
	}
	frameRes.Execute(nullptr);
	frameRes.Wait();
	for (auto&& i : disposeFuncs) {
		i();
	}
	float readbackValue = 0;
	readback.buffer->CopyValueTo(readbackValue, readback.offset);
	std::cout << "result[0] value: " << readbackValue << '\n';
}
void CompileAndTest(
	Device const* device,
	vstd::string const& shaderName,
	vstd::move_only_func<void(Device const* device, vstd::span<vbyte const> block)> const& func) {
	vstd::string shaderCode;
	{
		BinaryReader reader("test.compute");
		shaderCode = reader.ReadToString();
	}
	toolhub::directx::DXShaderCompiler compiler;
	auto compResult = compiler.CompileCompute(
		shaderCode,
		true);
	auto errMsg = compResult.try_get<vstd::string>();
	if (errMsg) {
		std::cout << "Compile error: " << *errMsg << '\n';
		return;
	}
	auto&& byteCode = compResult.get<0>();
	vstd::span<vbyte const> resultArr(
		byteCode->GetBufferPtr(),
		byteCode->GetBufferSize());
	func(device, resultArr);
}
int main() {
	auto instance = Device::InitVkInstance();
	auto device = Device::CreateDevice(instance, nullptr, 0);
	CompileAndTest(device, "test.compute", ComputeShaderTest);

	delete device;
	vkDestroyInstance(instance, Device::Allocator());
	std::cout << "finished\n";
	return 0;
}