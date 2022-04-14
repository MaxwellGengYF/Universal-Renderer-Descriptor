//
#include <Graphics/SDF/SDFPrimitives.h>
#include <Utility/MathLib.h>
namespace toolhub::graphics {
using namespace Math;
float SDFPrimitives::Distance(Vector3 const& vec, Capsule const& cap) {
	Vector3 pa = vec - (Vector3)cap.pointA, ba = (Vector3)cap.pointB - (Vector3)cap.pointA;
	float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
	return length(pa - ba * h) - cap.r;
}
float SDFPrimitives::Distance(Vector3 const& vec, Cube const& cb) {
	Matrix3 wtL(cb.right, cb.up, normalize(cross(cb.right, cb.up)));
	Vector3 p = vec - Vector3(cb.position);
	p = mul(wtL, p);
	Vector3 b = cb.localScale;
	Vector3 q = abs(p) - b;
	float v = length(Max(q, Vector3(0))) + std::min<float>(Max<float>(q.x, Max<float>(q.y, q.z)), 0.0);
	return v;
}
float SDFPrimitives::Distance(Vector3 const& vec, BBox const& bbox) {
	Vector3 const& p = vec - (Vector3)bbox.center;
	Vector3 b = bbox.extent;
	Vector3 q = abs(p) - b;
	return length(Max(q, Vector3(0))) + std::min<float>(Max<float>(q.x, Max<float>(q.y, q.z)), 0.0);
}

float SDFPrimitives::GetDistance(
	BVH* bvh,
	Vector3 pos) {

	CSSpan<PrimitiveIndices> primitives;
	CSSpan<BVHNode> bvhNodes;
	CSSpan<BBox> boxes;
	CSSpan<Cube> cubes;
	CSSpan<Capsule> capsules;
	if (!bvh->taskHandle) return 1e7;
	bvh->taskHandle->Complete();
	bvh->GetResult(
		&primitives,
		&bvhNodes,
		&boxes,
		&cubes,
		&capsules);
	return CalculateDistance(
		pos,
		primitives,
		bvhNodes,
		boxes,
		cubes,
		capsules);
}
bool SDFPrimitives::GetDistances(BVH* bvh, vstd::span<float3> poses, float* results) {
	CSSpan<PrimitiveIndices> primitives;
	CSSpan<BVHNode> bvhNodes;
	CSSpan<BBox> boxes;
	CSSpan<Cube> cubes;
	CSSpan<Capsule> capsules;
	if (!bvh->taskHandle) return false;
	bvh->taskHandle->Complete();
	bvh->GetResult(
		&primitives,
		&bvhNodes,
		&boxes,
		&cubes,
		&capsules);

	bvh->tPool
		->GetParallelTask<true>(
			[&](size_t i) {
				results[i] = CalculateDistance(
					poses[i],
					primitives,
					bvhNodes,
					boxes,
					cubes,
					capsules);
			},
			poses.size(),
			1)
		.Complete();
	return true;
}
float4 SDFPrimitives::GetNormalDistance(BVH* bvh, Math::Vector3 pos) {
	CSSpan<PrimitiveIndices> primitives;
	CSSpan<BVHNode> bvhNodes;
	CSSpan<BBox> boxes;
	CSSpan<Cube> cubes;
	CSSpan<Capsule> capsules;
	if (!bvh->taskHandle) return float4(0, 0, 0, 1e7);
	bvh->taskHandle->Complete();
	bvh->GetResult(
		&primitives,
		&bvhNodes,
		&boxes,
		&cubes,
		&capsules);
	return CalculateDistanceNormal(
		pos,
		primitives,
		bvhNodes,
		boxes,
		cubes,
		capsules);
}
bool SDFPrimitives::GetNormalDistances(BVH* bvh, vstd::span<float3> poses, float4* results) {
	CSSpan<PrimitiveIndices> primitives;
	CSSpan<BVHNode> bvhNodes;
	CSSpan<BBox> boxes;
	CSSpan<Cube> cubes;
	CSSpan<Capsule> capsules;
	if (!bvh->taskHandle) return false;
	bvh->taskHandle->Complete();
	bvh->GetResult(
		&primitives,
		&bvhNodes,
		&boxes,
		&cubes,
		&capsules);

	bvh->tPool
		->GetParallelTask<true>(
			[&](size_t i) {
				results[i] = CalculateDistanceNormal(
					poses[i],
					primitives,
					bvhNodes,
					boxes,
					cubes,
					capsules);
			},
			poses.size(),
			1)
		.Complete();
	return true;
}
float SDFPrimitives::CalculateDistance(
	Vector3 pos,
	CSSpan<PrimitiveIndices> primitives,
	CSSpan<BVHNode> bvhNodes,
	CSSpan<BBox> boxes,
	CSSpan<Cube> cubes,
	CSSpan<Capsule> capsules) {
	struct Stack {
		float minDist;
		uint id;
		Stack(float minDist, uint id) : minDist(minDist), id(id) {}
	};
	vstd::vector<Stack, VEngine_AllocType::Stack> vec;
	vec.reserve(64);
	vec.emplace_back(1e7f, 0);

	float resultDist = 1e7;
	while (!vec.empty()) {
		auto v = vec.erase_last();
		if (v.minDist > resultDist) continue;
		auto&& curNode = bvhNodes[v.id];
		auto IsLeaf = [](auto&& node) {
			return node.left_child_or_primitive < 0;
		};
		auto GetPrimitiveIndex = [](auto&& node) {
			return -node.left_child_or_primitive - 1;
		};
		auto GetRange = [](uint v) {
			uint ed = v & 65535;
			v >>= 16;
			return vstd::range(v, ed);
		};
		if (curNode.left_child_or_primitive < 0) {
			auto&& indices = primitives[GetPrimitiveIndex(curNode)];
			for (auto i : GetRange(indices.boxStartEnd)) {
				resultDist = Min(resultDist, Distance(pos, boxes[i]));
			}
			for (auto i : GetRange(indices.capStartEnd)) {
				resultDist = Min(resultDist, Distance(pos, capsules[i]));
			}
			for (auto i : GetRange(indices.cubeStartEnd)) {
				resultDist = Min(resultDist, Distance(pos, cubes[i]));
			}
		} else {
			auto&& leftNode = bvhNodes[curNode.left_child_or_primitive];
			auto&& rightNode = bvhNodes[curNode.left_child_or_primitive + 1];
			Stack leftStack(
				Distance(pos, leftNode.bbox),
				curNode.left_child_or_primitive);
			Stack rightStack(
				Distance(pos, rightNode.bbox),
				curNode.left_child_or_primitive + 1);
			bool leftAvaliable = leftStack.minDist < resultDist;
			bool rightAvaliable = rightStack.minDist < resultDist;
			if (leftAvaliable && rightAvaliable) {
				if (leftStack.minDist < rightStack.minDist) {
					vec.emplace_back(rightStack);
					vec.emplace_back(leftStack);
				} else {
					vec.emplace_back(leftStack);
					vec.emplace_back(rightStack);
				}
			} else if (leftAvaliable) {
				vec.emplace_back(leftStack);
			} else if (rightAvaliable) {
				vec.emplace_back(leftStack);
			}
		}
	}
	return resultDist;
}
float4 SDFPrimitives::CalculateDistanceNormal(Math::Vector3 pos, CSSpan<PrimitiveIndices> primitives, CSSpan<BVHNode> bvhNodes, CSSpan<BBox> boxes, CSSpan<Cube> cubes, CSSpan<Capsule> capsules) {
	struct Stack {
		float minDist;
		uint id;
		Stack(float minDist, uint id) : minDist(minDist), id(id) {}
	};
	vstd::vector<Stack, VEngine_AllocType::Stack> vec;
	vec.reserve(64);
	vec.emplace_back(1e7f, 0);

	std::pair<float, float3> resultDist{1e7f, float3(0, 0, 0)};
	const Vector2 e = Vector2(1.0, -1.0) * 0.5773 * 0.0005;
	auto GetNormalDistance = [&](auto&& distFunc, Vector3 pos) {
		auto Get = [&](float3 ee) {
			return distFunc(pos + Vector3(ee));
		};
		float dists[4];
		Vector3 samplePoses[4] = {
			Vector3(e.xyy),
			Vector3(e.yyx),
			Vector3(e.yxy),
			Vector3(e.xxx)};
		for (auto i : vstd::range(4)) {
			dists[i] = Get(samplePoses[i]);
			samplePoses[i] *= dists[i];
		}
		float dist = dot(Vector4(*(float4*)dists), Vector4(0.25f));
		if (dist < resultDist.first) {
			resultDist.first = dist;
			resultDist.second = normalize([&]() {
				Vector3 v(0);
				for (auto&& i : samplePoses) {
					v += i;
				}
				return v;
			}());
		}
	};

	while (!vec.empty()) {
		auto v = vec.erase_last();
		if (v.minDist > resultDist.first) continue;
		auto&& curNode = bvhNodes[v.id];
		auto IsLeaf = [](auto&& node) {
			return node.left_child_or_primitive < 0;
		};
		auto GetPrimitiveIndex = [](auto&& node) {
			return -node.left_child_or_primitive - 1;
		};
		auto GetRange = [](uint v) {
			uint ed = v & 65535;
			v >>= 16;
			return vstd::range(v, ed);
		};
		if (curNode.left_child_or_primitive < 0) {
			auto&& indices = primitives[GetPrimitiveIndex(curNode)];
			for (auto i : GetRange(indices.boxStartEnd)) {
				GetNormalDistance([&](auto&& pp) { return Distance(pp, boxes[i]); }, pos);
			}
			for (auto i : GetRange(indices.capStartEnd)) {
				GetNormalDistance([&](auto&& pp) { return Distance(pp, capsules[i]); }, pos);
			}
			for (auto i : GetRange(indices.cubeStartEnd)) {
				GetNormalDistance([&](auto&& pp) { return Distance(pp, cubes[i]); }, pos);
			}
		} else {
			auto&& leftNode = bvhNodes[curNode.left_child_or_primitive];
			auto&& rightNode = bvhNodes[curNode.left_child_or_primitive + 1];
			Stack leftStack(
				Distance(pos, leftNode.bbox),
				curNode.left_child_or_primitive);
			Stack rightStack(
				Distance(pos, rightNode.bbox),
				curNode.left_child_or_primitive + 1);
			bool leftAvaliable = leftStack.minDist < resultDist.first;
			bool rightAvaliable = rightStack.minDist < resultDist.first;
			if (leftAvaliable && rightAvaliable) {
				if (leftStack.minDist < rightStack.minDist) {
					vec.emplace_back(rightStack);
					vec.emplace_back(leftStack);
				} else {
					vec.emplace_back(leftStack);
					vec.emplace_back(rightStack);
				}
			} else if (leftAvaliable) {
				vec.emplace_back(leftStack);
			} else if (rightAvaliable) {
				vec.emplace_back(leftStack);
			}
		}
	}
	return Vector4(resultDist.second, resultDist.first);
}
float SDFPrimitives::Distance(Vector3 const& vec, Triangle const& tri) {
	return [](Vector3 const& p, Vector3 const& a, Vector3 const& b, Vector3 const& c) {
		auto sign = [](float v) {
			if (v == 0) return 0;
			if (v > 0) return 1;
			return -1;
		};
		auto dot2 = [](Vector3 const& v) {
			return dot(v, v);
		};
		Vector3 ba = b - a;
		Vector3 pa = p - a;
		Vector3 cb = c - b;
		Vector3 pb = p - b;
		Vector3 ac = a - c;
		Vector3 pc = p - c;
		Vector3 nor = cross(ba, ac);

		return sqrt(
			(sign(dot(cross(ba, nor), pa)) + sign(dot(cross(cb, nor), pb)) + sign(dot(cross(ac, nor), pc)) < 2.0f)
				? Min(Min(
						  dot2(ba * clamp(dot(ba, pa) / dot2(ba), 0.0, 1.0f) - pa),
						  dot2(cb * clamp(dot(cb, pb) / dot2(cb), 0.0, 1.0f) - pb)),
					  dot2(ac * clamp(dot(ac, pc) / dot2(ac), 0.0, 1.0f) - pc))
				: dot(nor, pa) * dot(nor, pa) / dot2(nor));
	}(
			   vec,
			   tri.vertex0,
			   tri.vertex1,
			   tri.vertex2);
}
VENGINE_UNITY_EXTERN float GetSDFDistance(
	BVH* bvh,
	float3 pos) {
	return SDFPrimitives::GetDistance(bvh, pos);
}

VENGINE_UNITY_EXTERN bool GetSDFDistances(
	BVH* bvh,
	float3* poses,
	uint posCount,
	float* distanceResults) {
	return SDFPrimitives::GetDistances(
		bvh,
		vstd::span<float3>(poses, posCount),
		distanceResults);
}

VENGINE_UNITY_EXTERN float4 GetSDFNormalDistance(
	BVH* bvh,
	float3 pos) {
	return SDFPrimitives::GetNormalDistance(bvh, pos);
}

VENGINE_UNITY_EXTERN bool GetSDFNormalDistances(
	BVH* bvh,
	float3* poses,
	uint posCount,
	float4* distanceResults) {
	return SDFPrimitives::GetNormalDistances(
		bvh,
		vstd::span<float3>(poses, posCount),
		distanceResults);
}

}// namespace toolhub::graphics
