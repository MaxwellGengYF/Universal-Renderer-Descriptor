#include <gpu_collection/buffer.h>
#include <shader/compute_shader.h>
#include <shader/shader_code.h>
#include <runtime/command_pool.h>
#include <runtime/frame_resource.h>
#include <runtime/res_state_tracker.h>

namespace toolhub::vk {
void TestBuffer(Device const* device, vstd::span<vbyte const> block) {
	struct Data {
		float4x4 mat;
		float4x4 mat1;
		float4 vec;
	};
	vstd::vector<float> v;
	v.push_back_func(sizeof(Data) / sizeof(float), [](size_t i) { return i + 1; });

	ShaderCode shaderCode(device, block, {});
	vstd::small_vector<VkDescriptorType> types;
	types.emplace_back(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	types.emplace_back(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	ComputeShader cs(
		device,
		shaderCode,
		types,
		uint3(1, 1, 1));
	ResStateTracker stateTracker(device);
	CommandPool cmdPool(device);
	FrameResource frameRes(device, &cmdPool);
	Buffer writeBuffer(
		device,
		sizeof(Data),
		false,
		RWState::None);
	BufferView readbackBuffer;
	if (auto cmdBuffer = frameRes.GetCmdBuffer()) {
		auto Cut = [&] {
			stateTracker.Execute(cmdBuffer);
			device->UpdateBindless();
			frameRes.ExecuteCopy();
		};

		auto inputBuffer = frameRes.AllocateDefault(sizeof(Data), device->limits.minStorageBufferOffsetAlignment);
		auto uploadBuffer = frameRes.AllocateUpload(sizeof(Data));
		readbackBuffer = frameRes.AllocateReadback(sizeof(Data));
		uploadBuffer.buffer->CopyFrom({reinterpret_cast<vbyte const*>(v.data()), v.byte_size()}, uploadBuffer.offset);
		stateTracker.MarkBufferWrite(
			inputBuffer,
			BufferWriteState::Copy);
		frameRes.AddCopyCmd(
			uploadBuffer.buffer,
			uploadBuffer.offset,
			inputBuffer.buffer,
			inputBuffer.offset,
			sizeof(Data));
		Cut();
		vstd::vector<BindResource> binds;
		binds.emplace_back(inputBuffer, false);
		binds.emplace_back(&writeBuffer, true);
		auto set = cmdBuffer->PreprocessDispatch(
			&cs,
			stateTracker,
			binds);
		Cut();
		cmdBuffer->Dispatch(
			set,
			&cs,
			uint3(1, 1, 1));

		stateTracker.MarkBufferRead(&writeBuffer, BufferReadState::ComputeOrCopy);
		frameRes.AddCopyCmd(
			&writeBuffer,
			0,
			readbackBuffer.buffer,
			readbackBuffer.offset,
			sizeof(Data));
		Cut();
	}
	frameRes.Execute(nullptr);
	frameRes.Wait();
	frameRes.Reset();
	Data resultData;
	readbackBuffer.buffer->CopyValueTo(resultData, readbackBuffer.offset);
	std::cout << "result: \n"
			  << "mat: ";
	auto PrintVec = [&](auto&& vec) {
		std::cout << (int)vec.x << ',' << (int)vec.y << ',' << (int)vec.z << ',' << (int)vec.w << '\n';
	};
	auto PrintMat = [&](auto&& mat) {
		for (auto i : vstd::range(4)) {
			PrintVec(mat[i]);
		}
	};
	PrintMat(resultData.mat);
	std::cout << "mat1: ";
	PrintMat(resultData.mat1);
	std::cout << "vec: ";
	PrintVec(resultData.vec);
}
}// namespace toolhub::vk