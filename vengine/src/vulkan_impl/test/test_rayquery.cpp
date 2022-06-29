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
#include <vulkan_impl/rtx/query.h>
namespace toolhub::vk {
void TestRayQuery(Device const* device, vstd::span<vbyte const> block) {
	ShaderCode shaderCode(device, block, {});
	vstd::small_vector<VkDescriptorType> types;
	types.emplace_back(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR);
	types.emplace_back(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	ComputeShader cs(
		device,
		shaderCode,
		types,
		uint3(5, 1, 1));
	ResStateTracker stateTracker(device);
	CommandPool cmdPool(device);
	FrameResource frameRes(device, &cmdPool);
	vstd::vector<vstd::move_only_func<void()>> disposeFuncs;
	vstd::move_only_func<void(vstd::move_only_func<void()> &&)> addDisposeEvent = [&](auto&& v) { disposeFuncs.push_back(std::move(v)); };
	BufferView readbackBuffer;
	Mesh mesh(device);
	Accel accel(device);
	struct alignas(16) Vertex {
		vec3 pos;
	};
	if (auto cmdBuffer = frameRes.GetCmdBuffer()) {
		auto Cut = [&] {
			stateTracker.Execute(cmdBuffer);
			device->UpdateBindless();
			frameRes.ExecuteCopy();
		};
		////////////////////////// upload mesh data
		auto vertUpload = frameRes.AllocateUpload(sizeof(Vertex) * 3);
		auto idxUpload = frameRes.AllocateUpload(sizeof(uint) * 3);
		auto vertBuffer = frameRes.AllocateDefault(sizeof(Vertex) * 3);
		auto idxBuffer = frameRes.AllocateDefault(sizeof(uint) * 3);
		auto writeBuffer = frameRes.AllocateDefault(sizeof(float2) * 5, device->limits.minStorageBufferOffsetAlignment);
		readbackBuffer = frameRes.AllocateReadback(sizeof(float2) * 5);
		vstd::array<Vertex, 3> verts;
		verts[0].pos = vec3(0, 0, 1);
		verts[1].pos = vec3(1, 0, 1);
		verts[2].pos = vec3(0.5, 1, 1);
		size_t id = 0;
		vstd::array<uint, 3> indices(vstd::MakeLazyEval([&] {
			return id++;
		}));
		vertUpload.buffer->CopyValueFrom(verts, vertUpload.offset);
		idxUpload.buffer->CopyValueFrom(indices, idxUpload.offset);
		stateTracker.MarkBufferWrite(vertBuffer, BufferWriteState::Copy);
		frameRes.AddCopyCmd(
			vertUpload.buffer,
			vertUpload.offset,
			vertBuffer.buffer,
			vertBuffer.offset,
			verts.byte_size);
		frameRes.AddCopyCmd(
			idxUpload.buffer,
			idxUpload.offset,
			idxBuffer.buffer,
			idxBuffer.offset,
			indices.byte_size);
		Cut();
		////////////////////////// build mesh
		{
			auto buildInfo = mesh.Preprocess(
				stateTracker,
				vertBuffer.buffer,
				sizeof(Vertex),
				vertBuffer.offset,
				vertBuffer.size,
				idxBuffer.buffer,
				idxBuffer.offset,
				idxBuffer.size,
				false,
				false,
				true,
				false,
				&frameRes);
			Buffer* scratchBuffer = new Buffer(
				device,
				buildInfo.scratchSize,
				false,
				RWState::None,
				0);
			stateTracker.MarkBufferWrite(
				scratchBuffer,
				BufferWriteState::Accel);
			buildInfo.buffer = scratchBuffer;
			buildInfo.scratchOffset = 0;
			frameRes.AddDisposeEvent([=] {
				delete scratchBuffer;
			});
			Cut();
			mesh.Build(
				cmdBuffer->CmdBuffer(),
				buildInfo,
				idxBuffer.size);
		}
		////////////////////////// update accel
		{
			size_t BUILD_SIZE = 1;
			auto buildInfo = accel.Preprocess(
				cmdBuffer,
				stateTracker,
				BUILD_SIZE,
				false,
				false,
				true,
				false,
				BUILD_SIZE,
				&frameRes);
			Buffer* scratchBuffer = new Buffer(
				device,
				buildInfo.scratchSize,
				false,
				RWState::None,
				0);
			stateTracker.MarkBufferWrite(
				scratchBuffer,
				BufferWriteState::Accel);
			buildInfo.buffer = scratchBuffer;
			buildInfo.scratchOffset = 0;
			frameRes.AddDisposeEvent([=] {
				delete scratchBuffer;
			});
			Cut();
			BufferView accelUpload = frameRes.AllocateUpload(
				BUILD_SIZE * Accel::ACCEL_INST_SIZE);
			size_t offset = accelUpload.offset;
			auto cpyCmd = accel.SetInstance(
				0,
				&mesh,
				float4x4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1),
				accelUpload.buffer,
				offset);
			accel.Build(
				stateTracker,
				cmdBuffer,
				buildInfo,
				accelUpload.buffer->GetResource(),
				vstd::span<VkBufferCopy>(&cpyCmd, 1),
				BUILD_SIZE);
		}
		////////////////////////// dispatch
		{
			vstd::vector<BindResource> binds;
			binds.emplace_back(&accel);
			binds.emplace_back(writeBuffer, true);
			auto set = cmdBuffer->PreprocessDispatch(
				&cs,
				stateTracker,
				binds);
			Cut();
			cmdBuffer->Dispatch(set, &cs, uint3(5, 1, 1));
		}
		////////////////////////// readback
		{
			stateTracker.MarkBufferRead(writeBuffer, BufferReadState::ComputeOrCopy);
			frameRes.AddCopyCmd(
				writeBuffer.buffer,
				writeBuffer.offset,
				readbackBuffer.buffer,
				readbackBuffer.offset,
				sizeof(float2) * 5);
			Cut();
		}
	}
	frameRes.Execute(nullptr);
	frameRes.Wait();
	frameRes.Reset();
	for (auto&& i : disposeFuncs) {
		i();
	}
	float2 value[5];
	readbackBuffer.buffer->CopyTo({reinterpret_cast<vbyte*>(value), vstd::array_byte_size(value)}, readbackBuffer.offset);
	for (auto i : value) {
		std::cout << i.x << ',' << i.y << '\n';
	}
}
}// namespace toolhub::vk