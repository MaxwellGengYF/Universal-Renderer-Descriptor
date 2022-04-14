#pragma once

#include <DirectX/DllManager.h>
#include <DirectX/Shader/ComputeShader.h>
#include <DirectX/Shader/RTShader.h>
#include <Graphics/ShaderCompiler/ShaderCompiler.h>
#include <DirectX/Runtime/Device.h>
#include <DirectX/Runtime/CommandQueue.h>
#include <DirectX/Runtime/CommandBuffer.h>
#include <DirectX/Runtime/CommandAllocator.h>
#include <DirectX/Resource/DefaultBuffer.h>
#include <DirectX/Resource/ReadbackBuffer.h>
#include <DirectX/Resource/UploadBuffer.h>
#include <DirectX/Resource/RenderTexture.h>
#include <DirectX/Runtime/ResourceStateTracker.h>
#include <DirectX/Resource/BindlessArray.h>
#include <DirectX/Resource/Mesh.h>
#include <DirectX/Resource/BottomAccel.h>
#include <DirectX/Resource/TopAccel.h>
#include <DirectX/Shader/ShaderSerializer.h>

int main() {
	using namespace toolhub;
	using namespace toolhub::directx;
	using namespace toolhub::graphics;
	Device device;
	auto dbBase = DllManager::GetDatabase();
	auto gra = DllManager::GetGraphics();
	auto stbi_write_jpg = DllManager::GetGraphicsDll()->GetDLLFunc<int(char const*, int, int, int, const void*, int)>("stbi_write_jpg");
	auto shader = ShaderSerializer::DeSerialize(
		&device,
		"shaders/MeshShadowMask.cso");
	auto rt = shader.template get_or<RTShader*>(nullptr);
	CommandQueue queue(&device, nullptr, D3D12_COMMAND_LIST_TYPE_COMPUTE);
	auto alloc = queue.CreateAllocator();
	DefaultBuffer outputBuffer(&device, 1024 * 1024 * sizeof(float3));
	constexpr size_t VERT_SIZE = sizeof(float3) * 4;
	constexpr size_t IDX_SIZE = sizeof(uint) * 3;
	DefaultBuffer vertBuffer(&device, VERT_SIZE + IDX_SIZE, nullptr);
	ResourceStateTracker stateTracker;
	auto dispatchCmd = alloc->GetBuffer();
	vstd::vector<float> floatResult;
	floatResult.resize(1024 * 1024 * 3);
	struct MeshStruct {
		float3 vert[4];
		uint idx[3];
	};
	static_assert(sizeof(MeshStruct) == VERT_SIZE + IDX_SIZE, "size must equal");
	MeshStruct mstr{
		.vert = {float3(0, 0, 1), float3(0.5f, 1, 1), float3(1, 0, 1), float3(0, 0, 0)},
		.idx = {0, 1, 2}};
	BottomAccel bAccel(
		&device,
		&vertBuffer,
		0,
		sizeof(float3),
		3,
		&vertBuffer,
		VERT_SIZE,
		3);
	TopAccel tAccel(
		&device);
	tAccel.Emplace(
		&bAccel,
		1,
		float4x4(1, 0, 0, 0.0f, 0, 1, 0, 0.05f, 0, 0, 1, 0.05f, 0, 0, 0, 1));
	//Compute
	{
		auto bf = dispatchCmd->Build();
		auto cmdlist = bf.CmdList();
		stateTracker.RecordState(
			&outputBuffer,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		stateTracker.RecordState(
			&vertBuffer,
			D3D12_RESOURCE_STATE_COPY_DEST);
		stateTracker.UpdateState(bf);
		bf.Upload(
			&vertBuffer,
			&mstr);
		stateTracker.RecordState(
			&vertBuffer);
		stateTracker.UpdateState(bf);
		bAccel.Build(
			bf);

		tAccel.Build(
			stateTracker,
			bf);
		bf.DispatchRT(
			rt,
			{1024, 1024, 1},
			{BindProperty{"_Result", &outputBuffer},
			 BindProperty{"_Scene", &tAccel}});
		stateTracker.RecordState(
			&outputBuffer,
			D3D12_RESOURCE_STATE_COPY_SOURCE);
		stateTracker.UpdateState(bf);
		bf.Readback(
			&outputBuffer,
			floatResult.data());
		stateTracker.RestoreState(bf);
	}
	queue.Complete(
		queue.Execute(std::move(alloc)));
	vstd::vector<vbyte> byteResult;
	byteResult.push_back_func(
		floatResult.size(),
		[&](size_t i) {
			return (vbyte)clamp(floatResult[i] * 255, 0, 255.99);
		});

	stbi_write_jpg("hdr_result.jpg", 1024, 1024, 3, reinterpret_cast<float*>(byteResult.data()), 100);
	return 0;
}