
#include <Graphics/bvh/BVH.h>
#include <Graphics/bvh/bvh.hpp>
#include <Graphics/bvh/vector.hpp>
#include <Utility/MathLib.h>
#include <Utility/QuickSort.h>
#include <Eigen/Eigen>
using namespace Math;
namespace toolhub::graphics {
using Vector3 = bvh::Vector<float, 3>;
Vector3 toVec(float3 v) {
	return {v.x, v.y, v.z};
};
float3 toFloat(Vector3 v) { return {v.value.x(), v.value.y(), v.value.z()}; }
void BVH::Execute() {
	AnalyzeBox();
	struct TempBVHNode {
		BBox bbox;
		uint primCount;
		uint left_child_or_primitive;
	};

	const uint boxAndCube = cubePrimitives.src.size() + boxPrimitives.src.size();
	auto Select = [&](
					  auto&& boxFunc,
					  auto&& cubeFunc,
					  auto&& capFunc,
					  uint index) {
		if (index < boxPrimitives.src.size()) {
			return boxFunc(index);
		} else if (index < boxAndCube) {
			return cubeFunc(index - boxPrimitives.src.size());
		} else {
			return capFunc(index - boxAndCube);
		}
	};

	auto GetBoundingBox = [&](uint index) {
		return Select(
			[&](uint index) {
				auto& bx = boxPrimitives.src[index];
				return std::pair<
					Vector3,
					Vector3>(
					toVec(bx.center) - toVec(bx.extent),
					toVec(bx.center) + toVec(bx.extent));
			},
			[&](uint index) {
				auto& bx = cubePrimitives.src[index];
				Vector3 verts[8];
				uint idx = 0;
				float v[2] = {1, -1};
				for (auto& x : vstd::range(2)) {
					Vector3 xOffset = toVec(bx.position) + toVec(bx.right) * v[x] * bx.localScale.x;
					for (auto& y : vstd::range(2)) {
						Vector3 yOffset = xOffset + toVec(bx.up) * v[y] * bx.localScale.y;
						for (auto& z : vstd::range(2)) {
							Vector3 fwd = cross(toVec(bx.right), (toVec(bx.up)));

							verts[idx] = yOffset + fwd * v[z] * bx.localScale.z;
							idx++;
						}
					}
				}
				Vector3 minV = verts[0];
				Vector3 maxV = verts[0];
				for (auto& i : vstd::range(1, 8)) {
					minV = bvh::min(minV, verts[i]);
					maxV = bvh::max(maxV, verts[i]);
				}
				return std::pair<Vector3, Vector3>(minV, maxV);
			},
			[&](uint index) {
				auto& cap = capPrimitives.src[index];
				Vector3 pA = toVec(cap.pointA);
				Vector3 pB = toVec(cap.pointB);
				Vector3 minV = bvh::min(pA, pB) - Vector3(cap.r);
				Vector3 maxV = bvh::max(pA, pB) + Vector3(cap.r);

				return std::pair<Vector3, Vector3>(minV, maxV);
			},
			index);
	};
	auto GetBVHCenter = [&](uint index) {
		return Select([&](uint index) {
			auto& bx = boxPrimitives.src[index];
			return toVec(bx.center); },
					  [&](uint index) {
						  auto& bx = cubePrimitives.src[index];
						  return toVec(bx.position);
					  },
					  [&](uint index) {
						  auto& cap = capPrimitives.src[index];
						  return toVec(cap.pointA) * 0.5f + toVec(cap.pointB) * 0.5f;
					  },
					  index);
	};
	auto UpdatePrimitive = [&](auto i) {
		auto ProcessPrimitive = [](auto&& prim, uint index) {
			prim.dest.emplace_back(prim.src[index]);
		};
		Select(
			[&](uint index) {
				ProcessPrimitive(boxPrimitives, index);
			},//Box
			[&](uint index) {
				ProcessPrimitive(cubePrimitives, index);
			},//Cube
			[&](uint index) {
				ProcessPrimitive(capPrimitives, index);
			},//Capsule
			i);
	};
	auto GetBVHBBox = [](std::pair<Vector3, Vector3> const& vec) {
		return bvh::BoundingBox<float>(
			vec.first,
			vec.second);
	};
	auto MinMaxToBBox = [&](std::pair<Vector3, Vector3> const& vec) {
		Vector3 halfMin = vec.first * 0.5f;
		Vector3 halfMax = vec.second * 0.5f;
		return BBox{
			toFloat(halfMin + halfMax),
			toFloat(halfMax - halfMin)};
	};
	primitiveIndices.clear();
	primitivesResults.clear();
	nodeResults.clear();
	const size_t sz = cubePrimitives.src.size() + capPrimitives.src.size() + boxPrimitives.src.size();
	if (sz == 0) return;
	libBvh.Clear();
	// Create an acceleration data structure on those triangles
	vstd::vector<bvh::BoundingBox<float>, VEngine_AllocType::Stack> boxes(sz);
	vstd::vector<bvh::Vector3<float>, VEngine_AllocType::Stack> centers(sz);
	std::pair<Vector3, Vector3> minMax = GetBoundingBox(0);
	minMax.first = Vector3(Float32MaxValue, Float32MaxValue, Float32MaxValue);
	minMax.second = Vector3(Float32MinValue, Float32MinValue, Float32MinValue);
	vstd::spin_mutex bboxMtx;
	auto prepareTask = tPool->GetBeginEndTask<true>(
		[&](size_t beg, size_t end) {
			Vector3 localMin(Float32MaxValue);
			Vector3 localMax(Float32MinValue);
			for (auto i : vstd::range(beg, end)) {
				auto bb = GetBoundingBox(i);
				boxes[i] = GetBVHBBox(bb);
				centers[i] = GetBVHCenter(i);
				localMin = bvh::min(localMin, bb.first);
				localMax = bvh::max(localMax, bb.second);
			}
			{
				std::lock_guard lck(bboxMtx);
				minMax.first = bvh::min(minMax.first, localMin);
				minMax.second = bvh::max(minMax.second, localMax);
			}
		},
		sz);
	prepareTask.Complete();
	builder.build(
		GetBVHBBox(minMax),
		boxes.data(),
		centers.data(),
		sz);
	globalBBox = MinMaxToBBox(minMax);
	primitiveIndices.resize(sz);
	auto const primNodes = libBvh.GetPrimitiveIndices();
	for (auto i : vstd::range(sz)) {
		primitiveIndices[i] = primNodes[i];
	}

	auto const bbNodes = libBvh.GetNodes();
	nodeResults.resize(libBvh.node_count);
	boxPrimitives.dest.clear();
	cubePrimitives.dest.clear();
	capPrimitives.dest.clear();
	for (auto i : vstd::range(libBvh.node_count)) {
		TempBVHNode tempNode;
		auto&& libNode = bbNodes[i];
		std::pair<Vector3, Vector3> minMax = {
			Vector3(
				libNode.bounds[0],
				libNode.bounds[2],
				libNode.bounds[4]),
			Vector3(
				libNode.bounds[1],
				libNode.bounds[3],
				libNode.bounds[5])};
		minMax.first *= 0.5f;
		minMax.second *= 0.5f;
		tempNode.bbox.center = toFloat(minMax.first + minMax.second);
		tempNode.bbox.extent = toFloat(minMax.second - minMax.first);

		tempNode.primCount = libNode.primitive_count;
		tempNode.left_child_or_primitive = libNode.first_child_or_primitive;
		auto&& bvhNode = nodeResults[i];
		bvhNode.bbox = tempNode.bbox;
		if (tempNode.primCount != 0) {
			bvhNode.left_child_or_primitive = -((int32)primitivesResults.size() + 1);
			auto&& primIdx = primitivesResults.emplace_back();
			uint boxStart = boxPrimitives.dest.size();
			uint cubeStart = cubePrimitives.dest.size();
			uint capStart = capPrimitives.dest.size();
			for (auto i : vstd::range(tempNode.left_child_or_primitive, tempNode.primCount + tempNode.left_child_or_primitive)) {
				UpdatePrimitive(primitiveIndices[i]);
			}
			uint boxEnd = boxPrimitives.dest.size();
			uint cubeEnd = cubePrimitives.dest.size();
			uint capEnd = capPrimitives.dest.size();
			auto PackUInt2 = [](uint x, uint y) {
				uint v = x & 65535;
				v <<= 16;
				v |= y & 65535;
				return v;
			};
			primIdx.boxStartEnd = PackUInt2(boxStart, boxEnd);
			primIdx.cubeStartEnd = PackUInt2(cubeStart, cubeEnd);
			primIdx.capStartEnd = PackUInt2(capStart, capEnd);
		} else {
			bvhNode.left_child_or_primitive = tempNode.left_child_or_primitive;
		}
	}
}
BVH::BVH(ThreadPool* tPool)
	: builder(libBvh) {
	this->tPool = tPool;
}

void BVH::SetPrimitives(
	CSSpan<Cube> cubes,
	CSSpan<Capsule> capsules) {
	cubePrimitives.src = cubes;
	capPrimitives.src = capsules;
}

void BVH::GetResult(
	CSSpan<PrimitiveIndices>* primitives,
	CSSpan<BVHNode>* bvhNodes,
	CSSpan<BBox>* boxes,
	CSSpan<Cube>* cubes,
	CSSpan<Capsule>* capsules) {
	auto SetSpan = [&](auto&& prim, auto&& vec) {
		prim->b = vec.begin();
		prim->e = vec.end();
	};
	SetSpan(primitives, primitivesResults);
	SetSpan(bvhNodes, nodeResults);
	SetSpan(boxes, boxPrimitives.dest);
	SetSpan(cubes, cubePrimitives.dest);
	SetSpan(capsules, capPrimitives.dest);
}
BVH::~BVH() {
}
void BVH::AnalyzeBox() {
	auto GetBBox = [](Cube const& cb) -> decltype(auto) {
		auto SetBit = [](Vector3 axis) {
			return ((axis.value.x() > 0.99999)
					|| (axis.value.y() > 0.99999)
					|| (axis.value.z() > 0.99999))
					   ? 1
					   : 0;
		};
		const Vector3 right = bvh::abs(toVec(cb.right));
		const Vector3 up = bvh::abs(toVec(cb.up));
		const bool isBox = (SetBit(right) + SetBit(up)) == 2;

		if (isBox) {
			const Vector3 extent =
				right * cb.localScale.x
				+ up * cb.localScale.y
				+ abs(cross(right, up)) * cb.localScale.z;
			return vstd::optional<BBox>().PlaceNew(
				cb.position,
				toFloat(extent));
		} else {
			return std::move(vstd::optional<BBox>());
		}
	};
	culledCubes.clear();
	culledBoxes.clear();
	for (auto cb : cubePrimitives.src) {
		auto bbx = GetBBox(cb);
		if (bbx) {
			culledBoxes.emplace_back(*bbx);
		} else {
			culledCubes.emplace_back(cb);
		}
	}
	cubePrimitives.src = CSSpan<Cube>(culledCubes.begin(), culledCubes.end());
	boxPrimitives.src = CSSpan<BBox>(culledBoxes.begin(), culledBoxes.end());
}
vstd::optional<Vector3> RayIntersectsTriangle(
	Vector3 rayOrigin,
	Vector3 rayVector,
	Triangle const& inTriangle,
	Vector3& outIntersectionPoint) {
	const float EPSILON = 0.0000001;
	Vector3 vertex0 = toVec(inTriangle.vertex0);
	Vector3 vertex1 = toVec(inTriangle.vertex1);
	Vector3 vertex2 = toVec(inTriangle.vertex2);
	Vector3 edge1, edge2, h, s, q;
	float a, f, u, v;
	edge1 = vertex1 - vertex0;
	edge2 = vertex2 - vertex0;
	h = cross(rayVector, edge2);
	a = dot(edge1, h);
	if (a > -EPSILON && a < EPSILON)
		return vstd::optional<Vector3>();// This ray is parallel to this triangle.
	f = 1.0 / a;
	s = rayOrigin - vertex0;
	u = f * dot(s, h);
	if (u < 0.0 || u > 1.0)
		return vstd::optional<Vector3>();
	q = cross(s, edge1);
	v = f * dot(rayVector, q);
	if (v < 0.0 || u + v > 1.0)
		return vstd::optional<Vector3>();
	// At this stage we can compute t to find out where the intersection point is on the line.
	float t = f * dot(edge2, q);
	if (t > EPSILON)// ray intersection
	{
		return {rayOrigin + rayVector * t};
	} else// This means that there is a line intersection but not a ray intersection.
		return vstd::optional<Vector3>();
}
vstd::optional<ThreadPool> Global_TPool;
VENGINE_UNITY_EXTERN size_t BVH_CreateGenerator() {
	Global_TPool.New(std::thread::hardware_concurrency());
	return reinterpret_cast<size_t>(new BVH(Global_TPool));
}

VENGINE_UNITY_EXTERN void BVH_DisposeGenerator(BVH* b) {
	if (b) {
		if (b->taskHandle) b->taskHandle->Complete();
		delete b;
	}
}

VENGINE_UNITY_EXTERN void BVH_GetBBox(BVH* handle, BBox* bbox) {
	*bbox = handle->GetBBox();
}

VENGINE_UNITY_EXTERN bool BVH_SetPrimitives(
	BVH* b,
	CSSpan<Cube> cubes,
	CSSpan<Capsule> capsules) {
	if (b->taskHandle)
		b->taskHandle->Complete();

	b->SetPrimitives(cubes, capsules);
	b->taskHandle = b->tPool->GetTask<true>([b]() {
		b->Execute();
	});
	b->taskHandle->Execute();
	return true;
}

VENGINE_UNITY_EXTERN bool BVH_GetResult(
	BVH* b,
	CSSpan<PrimitiveIndices>* primitives,
	CSSpan<BVHNode>* bvhNodes,
	CSSpan<BBox>* boxes,
	CSSpan<Cube>* cubes,
	CSSpan<Capsule>* capsules) {
	if (!b->taskHandle) return false;
	b->taskHandle->Complete();
	b->GetResult(primitives, bvhNodes, boxes, cubes, capsules);
	return true;
}
}// namespace toolhub::graphics