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
#include <Utility/BinaryReader.h>
#include <DirectX/Resource/BindlessArray.h>
#include <DirectX/Resource/Mesh.h>
#include <DirectX/Resource/BottomAccel.h>
#include <DirectX/Resource/TopAccel.h>
vstd::vector<vbyte> GetShader(
	vstd::string code,
	toolhub::db::IJsonDatabase* db) {
	using namespace toolhub;
	using namespace toolhub::directx;

	auto backCompute = R"(
void main(uint3 thdId : SV_GroupThreadId, uint3 dspId : SV_DispatchThreadID, uint3 grpId : SV_GroupId){
)"_sv;
	auto gra = DllManager::GetGraphics();
	auto compiler = vstd::create_unique(gra->CreateDirectXCompiler());
	auto inlineProperty = compiler->GetHLSLInlineProperty(code, db);
	auto preprocess = compiler->PreProcessHLSL(std::move(code));
	auto blkArray = inlineProperty->Get("Dispatch").template get_or<db::IJsonArray*>(nullptr);
	uint3 blockSize = inlineProperty ? ((blkArray && blkArray->Length() >= 3)
											? uint3(
												blkArray->Get(0).template get_or<int64>(1),
												blkArray->Get(1).template get_or<int64>(1),
												blkArray->Get(2).template get_or<int64>(1))
											: uint3(1, 1, 1))
									 : uint3(1, 1, 1);
	auto entry = inlineProperty->Get("Entry").template get_or<vstd::string_view>("run"_sv);
	preprocess.resultCode
		<< "[numthreads("_sv
		<< vstd::to_string(blockSize.x) << ','
		<< vstd::to_string(blockSize.y) << ','
		<< vstd::to_string(blockSize.z)
		<< ")]\n"
		<< backCompute
		<< entry
		<< "(thdId, dspId, grpId);}"_sv;
	auto result = compiler->CompileCompute(
		preprocess.resultCode,
		true,
		65);
	if (!result.multi_visit_or(
			false,
			[&](vstd::unique_ptr<graphics::IByteBlob> const& b) {
				std::cout << "DXIL size: "_sv << b->GetBufferSize() << '\n';
				return true;
			},
			[&](vstd::string const& b) {
				std::cout << b << '\n';
				return false;
			})) return {};
	auto res = vstd::create_unique(
		static_cast<graphics::IComputeShader*>(
			gra->CreateResource(graphics::IResource::Tag::ComputeShader)));
	res->binBytes = std::move(result).template get<0>();
	res->blockSize = blockSize;
	res->properties = std::move(preprocess.properties);
	vstd::vector<vbyte> bt;
	res->Save(bt, db);
	return bt;
}

int main() {
	using namespace toolhub;
	using namespace toolhub::directx;
	Device device;
	auto dbBase = DllManager::GetDatabase();
	auto db = vstd::create_unique(dbBase->CreateDatabase());
	auto gra = DllManager::GetGraphics();
	BinaryReader reader("shaders/testRTX.compute");
	auto fileCode = reader.ReadToString();
	auto compileResult = GetShader(std::move(fileCode), db.get());
	if (compileResult.size() == 0) return 1;
	return 0;
	auto res = vstd::create_unique(
		static_cast<graphics::IComputeShader*>(
			gra->CreateResource(db.get(), compileResult)));

	ComputeShader cs(res.get(), device.device.Get());
	CommandQueue queue(&device, nullptr, D3D12_COMMAND_LIST_TYPE_COMPUTE);

	auto alloc = queue.CreateAllocator();

	DefaultBuffer outputBuffer(&device, sizeof(uint4));

	constexpr size_t VERT_SIZE = sizeof(float3) * 4;
	constexpr size_t IDX_SIZE = sizeof(uint) * 3;
	DefaultBuffer vertBuffer(&device, VERT_SIZE + IDX_SIZE, nullptr);
	struct TestBuffer {
		bool a[4];
	};
	uint4 outputResult;
	vstd::vector<TestBuffer> floatResult(1);
	for (auto i : vstd::range(floatResult.size())) {
		floatResult[i] = TestBuffer{
			.a = {false,
				  true,
				  true,
				  true}};
	}
	DefaultBuffer inputBuffer(&device, floatResult.byte_size());

	struct MeshStruct {
		float3 vert[4];
		uint idx[3];
	};
	static_assert(sizeof(MeshStruct) == VERT_SIZE + IDX_SIZE, "size must equal");
	MeshStruct mstr{
		.vert = {float3(0, 0, 1), float3(0.5f, 1, 1), float3(1, 0, 1), float3(0, 0, 0)},
		.idx = {0, 2, 1}};

	ResourceStateTracker stateTracker;
	auto dispatchCmd = alloc->GetBuffer();
	BottomAccel bAccel(
		&device,
		&vertBuffer,
		0,
		sizeof(float3),
		3,
		&vertBuffer,
		VERT_SIZE,
		3);
	{
		TopAccel tAccel(
			&device);
		tAccel.Emplace(
			&bAccel,
			1,
			float4x4(1, 0, 0, 0.05f, 0, 1, 0, 0.05f, 0, 0, 1, 0.05f, 0, 0, 0, 1));
		//Compute
		{
			auto bf = dispatchCmd->Build();
			auto cmdlist = bf.CmdList();
			stateTracker.RecordState(
				&outputBuffer,
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			stateTracker.RecordState(
				&inputBuffer,
				D3D12_RESOURCE_STATE_COPY_DEST);

			stateTracker.RecordState(
				&vertBuffer,
				D3D12_RESOURCE_STATE_COPY_DEST);
			stateTracker.UpdateState(bf);
			bf.Upload(
				&inputBuffer,
				floatResult.data());
			bf.Upload(
				&vertBuffer,
				&mstr);
			stateTracker.RecordState(
				&vertBuffer);
			stateTracker.RecordState(
				&inputBuffer);
			stateTracker.UpdateState(bf);
			bAccel.Build(
				bf);
			tAccel.Build(
				stateTracker,
				bf);
			bf.DispatchCompute(
				&cs,
				{1, 1, 1},
				{BindProperty{"_Result", &outputBuffer},
				 BindProperty{"_CB", &inputBuffer}});
			stateTracker.RecordState(
				&outputBuffer,
				D3D12_RESOURCE_STATE_COPY_SOURCE);
			stateTracker.UpdateState(bf);
			bf.Readback(
				&outputBuffer,
				&outputResult);
			stateTracker.RestoreState(bf);
		}
		queue.Complete(
			queue.Execute(std::move(alloc)));
	}
	std::cout
		<< outputResult.x << ' '
		<< outputResult.y << ' '
		<< outputResult.z << ' '
		<< outputResult.w;
	return 0;
	/*
	vstd::vector<vbyte> byteResult;
	byteResult.push_back_func(
		[&](size_t i) {
			return (vbyte)clamp(floatResult[i] * 255, 0, 255.99);
		},
		floatResult.size());

	stbi_write_jpg("hdr_result.jpg", 1024, 1024, 3, reinterpret_cast<float*>(byteResult.data()), 100);*/
}