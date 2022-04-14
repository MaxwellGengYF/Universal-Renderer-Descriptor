#pragma once
#include <Common/Common.h>
#include <span>
#include <Graphics/bvh/sweep_sah_builder.hpp>
#include <JobSystem/ThreadPool.h>
#include <Common/DXMath/DXMath.h>

namespace toolhub::graphics {
struct BBox {
	float3 center;
	float3 extent;
};

struct Cube {
	float3 right;
	float3 up;
	float3 localScale;
	float3 position;
};

struct Capsule {
	float3 pointA;
	float3 pointB;
	float r;
};
struct Triangle {
	float3 vertex0;
	float3 vertex1;
	float3 vertex2;
};
struct Sphere {
	float3 center;
	float radius;
};

struct BVHNode {
	BBox bbox;
	int32 left_child_or_primitive;
};
struct Cylinder {
	float3 pointA;
	float3 pointB;
	float r;
};
struct Plane {
	float3 normal;
	float h;
};
struct PrimitiveIndices {
	uint boxStartEnd;
	uint cubeStartEnd;
	uint capStartEnd;
};
template<typename T>
struct CSSpan {
	T* b;
	T* e;
	CSSpan() {}
	CSSpan(
		T* b,
		T* e) : b(b), e(e) {}
	T* begin() const {
		return b;
	}
	T* end() const {
		return e;
	}
	T& operator[](size_t i) const {
		return b[i];
	}
	size_t size() const {
		return e - b;
	}
};
class BVH : public vstd::IOperatorNewBase {

public:
	void Execute();
	void AnalyzeBox();
	BVH(ThreadPool* tPool);
	~BVH();
	
	void SetPrimitives(
		CSSpan<Cube> cubes,
		CSSpan<Capsule> capsules);
	void GetResult(
		CSSpan<PrimitiveIndices>* primitives,
		CSSpan<BVHNode>* bvhNodes,
		CSSpan<BBox>* boxes,
		CSSpan<Cube>* cubes,
		CSSpan<Capsule>* capsules);

	vstd::optional<ThreadTaskHandle<true>> taskHandle;
	ThreadPool* tPool;
	template<typename T>
	struct Primitive {
		CSSpan<T> src;
		vstd::vector<T> dest;
	};
	BBox const& GetBBox() const {
		return globalBBox;
	}

private:
	Primitive<BBox> boxPrimitives;
	Primitive<Cube> cubePrimitives;
	Primitive<Capsule> capPrimitives;
	vstd::vector<Cube> culledCubes;
	vstd::vector<BBox> culledBoxes;

	vstd::vector<BVHNode> nodeResults;
	vstd::vector<uint> primitiveIndices;
	vstd::vector<PrimitiveIndices> primitivesResults;
	bvh::Bvh<float> libBvh;
	BBox globalBBox;
	bvh::SweepSahBuilder<bvh::Bvh<float>> builder;
};
}// namespace toolhub::graphics