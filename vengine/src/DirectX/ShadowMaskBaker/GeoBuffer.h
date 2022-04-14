#pragma once
#include <Common/Common.h>
#include <DirectX/Runtime/CommandBuffer.h>
#include <DirectX/Runtime/ResourceStateTracker.h>
#include <DirectX/Shader/ComputeShader.h>
#include <DirectX/Shader/RTShader.h>
namespace toolhub::directx {
class DefaultBuffer;
class BottomAccel;
class TopAccel;
class GeoBuffer : public vstd::IOperatorNewBase {
public:
	struct Vertex {
		float3 position;
		float3 normal;
		float4 tangent;
		float2 uv0;
		float2 uv1;
	};
	Device* device;
	// Mesh
	vstd::vector<Vertex> verts;
	vstd::vector<float3> uvVerts;
	vstd::vector<uint> idices;
	vstd::unique_ptr<RTShader> projShader;
	vstd::unique_ptr<DefaultBuffer> vertBuffer;
	vstd::unique_ptr<DefaultBuffer> mappedBuffer;
	vstd::unique_ptr<DefaultBuffer> idxBuffer;
	vstd::unique_ptr<BottomAccel> vertBottom;
	vstd::unique_ptr<TopAccel> vertTop;
	vstd::unique_ptr<BottomAccel> mapBottom;
	vstd::unique_ptr<TopAccel> mapTop;
	// Texture
	GeoBuffer(
	Device* device);
	void InputData(
		float3* positions,
		float3* normals,
		float4* tangents,
		float2* uv,
		float2* uv1,
		size_t vertCount,
		uint* indices,
		size_t indCount);
	void Compute(
		DefaultBuffer const& outputBuffer,
		void* result,
		CommandBufferBuilder& cb,
		ResourceStateTracker& tracker);
	~GeoBuffer();

private:
	void DrawRayTracing(
		DefaultBuffer const& outputBuffer,
		CommandBufferBuilder& cb,
		ResourceStateTracker& tracker);
};
}// namespace toolhub::directx