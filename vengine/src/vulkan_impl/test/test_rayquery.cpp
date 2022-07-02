#include <shader/compute_shader.h>
#include <shader/shader_code.h>
#include <gpu_collection/buffer.h>
#include <shader/descriptor_pool.h>
#include <shader/descriptorset_manager.h>
#include <runtime/command_pool.h>
#include <runtime/frame_resource.h>
#include <runtime/res_state_tracker.h>
#include <gpu_collection/texture.h>
#include <rtx/mesh.h>
#include <rtx/accel.h>
#include <rtx/query.h>
#include <runtime/render_pipeline.h>
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
	RenderPipeline rp(device);
	BufferView readbackBuffer;
	Mesh mesh(device);
	Accel accel(device);
	struct alignas(16) Vertex {
		vec3 pos;
	};
	auto& stateTracker = rp.StateTracker();
	Buffer writeBuffer(
		device,
		sizeof(float2) * 5,
		false,
		RWState::None);
	auto ExecuteFunc = [&](FrameResource& frameRes) {
		if (auto cmdBuffer = frameRes.GetCmdBuffer()) {
			auto Cut = [&] {
				frameRes.ExecuteScratchAlloc(stateTracker);
				stateTracker.Execute(cmdBuffer);
				device->UpdateBindless();
				frameRes.ExecuteCopy();
			};
			////////////////////////// upload mesh data
			auto vertUpload = frameRes.AllocateUpload(sizeof(Vertex) * 3);
			auto idxUpload = frameRes.AllocateUpload(sizeof(uint) * 3);
			auto vertBuffer = frameRes.AllocateDefault(sizeof(Vertex) * 3);
			auto idxBuffer = frameRes.AllocateDefault(sizeof(uint) * 3);
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
				frameRes.ResetScratch();
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
				frameRes.AddScratchSize(buildInfo.scratchSize);
				Cut();
				auto scratchBuffer = frameRes.GetScratchBufferView();
				buildInfo.buffer = scratchBuffer.buffer;
				buildInfo.scratchOffset = scratchBuffer.offset;
				mesh.Build(
					cmdBuffer->CmdBuffer(),
					buildInfo,
					idxBuffer.size);
			}
			////////////////////////// update accel
			{
				frameRes.ResetScratch();
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
				frameRes.AddScratchSize(buildInfo.scratchSize);
				Cut();
				auto scratchBuffer = frameRes.GetScratchBufferView();
				buildInfo.buffer = scratchBuffer.buffer;
				buildInfo.scratchOffset = scratchBuffer.offset;
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
				binds.emplace_back(&writeBuffer, true);
				auto set = cmdBuffer->PreprocessDispatch(
					&cs,
					stateTracker,
					binds);
				Cut();
				cmdBuffer->Dispatch(set, &cs, uint3(5, 1, 1));
			}
			////////////////////////// readback
			{
				stateTracker.MarkBufferRead(&writeBuffer, BufferReadState::ComputeOrCopy);
				frameRes.AddCopyCmd(
					&writeBuffer,
					0,
					readbackBuffer.buffer,
					readbackBuffer.offset,
					sizeof(float2) * 5);
				Cut();
			}
		}
	};
	{
		auto& frameRes = *rp.BeginPrepareFrame();
		ExecuteFunc(frameRes);
		rp.EndPrepareFrame();
	}
	{
		auto& frameRes = *rp.BeginPrepareFrame();
		ExecuteFunc(frameRes);
		rp.EndPrepareFrame();
	}
	{
		auto& frameRes = *rp.BeginPrepareFrame();
		ExecuteFunc(frameRes);
		rp.EndPrepareFrame();
	}
	rp.Complete();

	float2 value[5];
	readbackBuffer.buffer->CopyTo({reinterpret_cast<vbyte*>(value), vstd::array_byte_size(value)}, readbackBuffer.offset);
	for (auto i : value) {
		std::cout << i.x << ',' << i.y << '\n';
	}
}
}// namespace toolhub::vk