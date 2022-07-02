#include <gpu_collection/buffer.h>
#include <shader/compute_shader.h>
#include <shader/shader_code.h>
#include <runtime/command_pool.h>
#include <runtime/frame_resource.h>
#include <runtime/res_state_tracker.h>
#include <gpu_collection/bindless_array.h>

namespace toolhub::vk {
void TestBindless(Device const* device, vstd::span<vbyte const> block) {
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
	BufferView readback;
	BindlessArray arr(device, 256);
	arr.Bind(244, &defaultBuffer, 0);
	if (auto cmdBuffer = frameRes.GetCmdBuffer()) {
		auto Cut = [&] {
			stateTracker.Execute(cmdBuffer);
			device->UpdateBindless();
			frameRes.ExecuteCopy();
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
	frameRes.Reset();
	float readbackValue = 0;
	readback.buffer->CopyValueTo(readbackValue, readback.offset);
	std::cout << "result[0] value: " << readbackValue << '\n';
}
}// namespace toolhub::vk