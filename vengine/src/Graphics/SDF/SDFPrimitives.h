#pragma once
#include <Graphics/BVH/BVH.h>
namespace toolhub::graphics {
class SDFPrimitives {
public:
	static float Distance(
		Math::Vector3 const& vec,
		Capsule const& cap);
	static float Distance(
		Math::Vector3 const& vec,
		Cube const& cb);
	static float Distance(
		Math::Vector3 const& vec,
		BBox const& bbox);
	static float Distance(
		Math::Vector3 const& vec,
		Triangle const& tri);
	static float GetDistance(
		BVH* bvh,
		Math::Vector3 pos);
	static bool GetDistances(
		BVH* bvh,
		vstd::span<float3> poses,
		float* results);
	static float4 GetNormalDistance(
		BVH* bvh,
		Math::Vector3 pos);
	static bool GetNormalDistances(
		BVH* bvh,
		vstd::span<float3> poses,
		float4* results);
	static float CalculateDistance(
		Math::Vector3 pos,
		CSSpan<PrimitiveIndices> primitives,
		CSSpan<BVHNode> bvhNodes,
		CSSpan<BBox> boxes,
		CSSpan<Cube> cubes,
		CSSpan<Capsule> capsules);
	static float4 CalculateDistanceNormal(
		Math::Vector3 pos,
		CSSpan<PrimitiveIndices> primitives,
		CSSpan<BVHNode> bvhNodes,
		CSSpan<BBox> boxes,
		CSSpan<Cube> cubes,
		CSSpan<Capsule> capsules);
	/*
	float udTriangle( vec3 p, vec3 a, vec3 b, vec3 c )
{
  vec3 ba = b - a; vec3 pa = p - a;
  vec3 cb = c - b; vec3 pb = p - b;
  vec3 ac = a - c; vec3 pc = p - c;
  vec3 nor = cross( ba, ac );

  return sqrt(
    (sign(dot(cross(ba,nor),pa)) +
     sign(dot(cross(cb,nor),pb)) +
     sign(dot(cross(ac,nor),pc))<2.0)
     ?
     min( min(
     dot2(ba*clamp(dot(ba,pa)/dot2(ba),0.0,1.0)-pa),
     dot2(cb*clamp(dot(cb,pb)/dot2(cb),0.0,1.0)-pb) ),
     dot2(ac*clamp(dot(ac,pc)/dot2(ac),0.0,1.0)-pc) )
     :
     dot(nor,pa)*dot(nor,pa)/dot2(nor) );
}
	*/
};
}// namespace toolhub::graphics