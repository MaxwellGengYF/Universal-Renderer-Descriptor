#pragma once
#include <Graphics/SceneDescriptor/IResource.h>
namespace toolhub::graphics {
class IMesh : public IResource {
public:
	struct Vertices {
		vstd::span<float3 const> position;
		vstd::span<float3 const> normal;
		vstd::span<float4 const> tangent;
		vstd::span<float4 const> color;
		vstd::span<vstd::span<float2 const> const> uvs;
	};
	virtual Vertices GetVertices() const = 0;
	virtual void UpdateVertices(
		vstd::span<float3 const> position,
		vstd::span<float3 const> normal,
		vstd::span<float4 const> tangent,
		vstd::span<float4 const> color,
		vstd::span<vstd::span<float2 const>> uvs) = 0;
	virtual vstd::span<uint const> GetIndices() const = 0;
	virtual void UpdateIndices(vstd::span<uint const> indices) = 0;

	virtual ~IMesh() = default;
};
}// namespace toolhub::graphics