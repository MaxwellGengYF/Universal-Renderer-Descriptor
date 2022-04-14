
#include <DirectX/ShadowMaskBaker/GeoBuffer.h>
#include <DirectX/DllManager.h>

#include <DirectX/Resource/DefaultBuffer.h>
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
#include <DirectX/Resource/BottomAccel.h>
#include <DirectX/Resource/TopAccel.h>
namespace toolhub::directx {
static float4x4 Identity() {
	return float4x4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
}
GeoBuffer::GeoBuffer(Device* device)
	: device(device) {
	auto deserResult = ShaderSerializer::DeSerialize(
		device,
		"shaders/MeshShadowMask1.cso");
	projShader = vstd::create_unique(deserResult.template get_or<RTShader*>(nullptr));
}
template<typename T>
static decltype(auto) GetZero() {
	if constexpr (std::is_same_v<T, float3>)
		return float3(0, 0, 0);
	else if constexpr (std::is_same_v<T, float2>)
		return float2(0, 0);
	else if constexpr (std::is_same_v<T, float4>)
		return float4(0, 0, 0, 0);
}
void GeoBuffer::InputData(
	float3* positions,
	float3* normals,
	float4* tangents,
	float2* uv0s,
	float2* uv1s,
	size_t vertCount,
	uint* indices,
	size_t indCount) {
	verts.resize(vertCount);
#define EXECUTE(v, vs)                                             \
	if (vs) {                                                      \
		for (auto i : vstd::range(vertCount)) {                    \
			auto&& va = verts[i];                                  \
			va.v = vs[i];                                          \
		}                                                          \
	} else {                                                       \
		for (auto i : vstd::range(vertCount)) {                    \
			auto&& va = verts[i];                                  \
			va.v = GetZero<std::remove_cvref_t<decltype(va.v)>>(); \
		}                                                          \
	}
	EXECUTE(position, positions)
	EXECUTE(normal, normals)
	EXECUTE(tangent, tangents)
	EXECUTE(uv0, uv0s)
	EXECUTE(uv1, uv1s)
#undef EXECUTE
	uvVerts.push_back_func(
		vertCount,
		[&](size_t i) {
			auto&& a = uv1s[i];
			return float3(a.x, a.y, 1);
		});
	idices.resize(indCount);
	memcpy(idices.data(), indices, idices.byte_size());
	vertBuffer = vstd::create_unique(new DefaultBuffer(
		device,
		verts.byte_size(),
		nullptr,
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));
	mappedBuffer = vstd::create_unique(new DefaultBuffer(
		device,
		uvVerts.byte_size(),
		nullptr,
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));
	idxBuffer = vstd::create_unique(new DefaultBuffer(
		device,
		idices.byte_size(),
		nullptr,
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));
	vertBottom = vstd::create_unique(new BottomAccel(
		device,
		vertBuffer.get(),
		0,
		sizeof(Vertex),
		verts.size(),
		idxBuffer.get(),
		0,
		idices.size()));
	vertTop = vstd::create_unique(new TopAccel(
		device));
	vertTop->Emplace(
		vertBottom.get(),
		1,
		Identity());
	mapBottom = vstd::create_unique(new BottomAccel(
		device,
		mappedBuffer.get(),
		0,
		sizeof(float3),
		verts.size(),
		idxBuffer.get(),
		0,
		idices.size()));
	mapTop = vstd::create_unique(new TopAccel(
		device));
	mapTop->Emplace(
		mapBottom.get(),
		1,
		Identity());
}
void GeoBuffer::Compute(
	DefaultBuffer const& outputBuffer,
	void* result,
	CommandBufferBuilder& cb,
	ResourceStateTracker& tracker) {
	tracker.RecordState(&outputBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	tracker.RecordState(vertBuffer.get(), D3D12_RESOURCE_STATE_COPY_DEST);
	tracker.RecordState(mappedBuffer.get(), D3D12_RESOURCE_STATE_COPY_DEST);
	tracker.RecordState(idxBuffer.get(), D3D12_RESOURCE_STATE_COPY_DEST);
	tracker.UpdateState(cb);
	cb.Upload(
		vertBuffer.get(),
		verts.data());
	cb.Upload(
		mappedBuffer.get(),
		uvVerts.data());
	cb.Upload(idxBuffer.get(), idices.data());
	tracker.RecordState(vertBuffer.get());
	tracker.RecordState(mappedBuffer.get());
	tracker.RecordState(idxBuffer.get());
	tracker.UpdateState(cb);
	vertBottom->Build(cb);
	mapBottom->Build(cb);
	vertTop->Build(tracker, cb);
	mapTop->Build(tracker, cb);
	DrawRayTracing(outputBuffer, cb, tracker);
	tracker.RecordState(
		&outputBuffer,
		D3D12_RESOURCE_STATE_COPY_SOURCE);
	tracker.UpdateState(cb);
	cb.Readback(
		&outputBuffer,
		result);
	tracker.RestoreState(cb);
}
void GeoBuffer::DrawRayTracing(
	DefaultBuffer const& outputBuffer,
	CommandBufferBuilder& cb,
	ResourceStateTracker& tracker) {
	cb.DispatchRT(
		projShader.get(),
		uint3(1024, 1024, 1),
		{BindProperty{"_Mesh", vertTop->GetAccelBuffer()},
		 BindProperty{"_MapMesh", mapTop->GetAccelBuffer()},
		 BindProperty{"_Result", &outputBuffer},
		 BindProperty{"_Vertices", vertBuffer.get()},
		 BindProperty{"_Indices", idxBuffer.get()}});
}
GeoBuffer::~GeoBuffer() {}
}// namespace toolhub::directx