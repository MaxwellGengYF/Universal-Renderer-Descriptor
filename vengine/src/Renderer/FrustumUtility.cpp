#include <Renderer/FrustumCulling.h>
#include <Utility/TaskThread.h>
#include <Utility/MathLib.h>
namespace toolhub::renderer {
mat4 GetCSMTransformMat(const vec3& right, const vec3& up, const vec3& forward, const vec3& position) {
	mat4 target;
	target[0] = vec4(right, 0);
	target[1] = vec4(up, 0);
	target[2] = vec4(forward, 0);
	target[3] = vec4(position, 1);
	return target;
}
static constexpr size_t DIR_COUNT = 5;
struct LightBBox {
	vec3 position;
	float planeSize;
};
LightBBox GetFrustumLightBBox(
	mat4 const& w2l,
	float farDist,
	std::array<vec3, DIR_COUNT> const& frustumRays,
	vstd::span<vec3> frustumPoints) {
	float depthOffset = (1 - dot(frustumRays[0], frustumRays[DIR_COUNT - 1])) * farDist;
	float maxDist = 0;
	vec3 minPos(1e7f);
	vec3 maxPos(-1e7f);
	for (auto start : vstd::range(frustumPoints.size())) {
		auto&& v = frustumPoints[start];
		v = w2l * vec4(v, 1);
		for (auto end : vstd::range(start + 1, frustumPoints.size())) {
			maxDist = max(maxDist, distance(v, frustumPoints[end]));
		}
		minPos = min(minPos, v);
		maxPos = max(maxPos, v);
	}
	vec2 extent(maxPos - minPos);
	vec2 leftSize = vec2(maxDist) - extent;
	vec2 xyCenter = vec2(minPos) + maxDist * 0.5f - leftSize * 0.5f;
	return LightBBox{
		.position = vec3(xyCenter, maxPos.z + depthOffset),
		.planeSize = maxDist};
}
static std::array<vec3, DIR_COUNT> GetFrustumRay(
	vec3 const& right,
	vec3 const& up,
	vec3 const& forward,
	float fov,
	float aspect) {
	vec3 originDir = forward;
	float rate = tan(fov * 0.5f);
	vec3 upDir = up * rate;
	vec3 rightDir = right * rate * aspect;
	return {
		normalize(forward - upDir - rightDir),
		normalize(forward + upDir - rightDir),
		normalize(forward - upDir + rightDir),
		normalize(forward + upDir + rightDir),
		forward};
}
static std::pair<vec3, vec3> GetBounding(
	mat4 const& sunWtL,
	std::array<vec3, DIR_COUNT> const& dirs,
	vec3 const& camPos) {
	vec3 minPos(1e7f);
	vec3 maxPos(-1e7f);
	for (auto&& i : dirs) {
		auto p = vec3(sunWtL * vec4(camPos, 1));
		minPos = min(minPos, p);
		maxPos = max(maxPos, p);
	}
}
static vec4 GetCascadeSphere(
	vec3 pos,
	std::array<vec3, DIR_COUNT> const& dirs,
	float nearDist,
	float farDist) {
	std::array<vec3, DIR_COUNT * 2> poses;
	for (auto i : vstd::range(DIR_COUNT)) {
		poses[i] = pos + dirs[i] * nearDist;
		poses[i + DIR_COUNT] = pos + dirs[i] * farDist;
	}
	vec3 center(0);
	for (auto&& v : poses) {
		center += v / vec3(poses.size());
	}
	float dist = 0;
	for (auto&& v : poses) {
		dist = max(dist, distance(v, center));
	}
	return vec4(center, dist);
}

void GetCascadeShadowmapMatrices(
	mat4 const& sunLocalToWorld,
	vec3 const& cameraRight,
	vec3 const& cameraUp,
	vec3 const& cameraForward,
	vec3 const& cameraPosition,
	float fov,
	float aspect,
	float resolution,
	float nearPlane,
	vstd::span<FrustumCulling::ShadowmapData> results,
	std::array<vec4, 6>* cameraFrustumPlanes,
	std::pair<vec3, vec3>* camFrustumMinMax) {

	mat4 sunWorldToLocal = inverse(sunLocalToWorld);
	auto rays = GetFrustumRay(cameraRight, cameraUp, cameraForward, fov, aspect);
	for (auto i : vstd::range(results.size())) {
		auto&& result = results[i];
		float nearDist;
		float farDist = result.distance;
		if (i == 0)
			nearDist = nearPlane;
		else
			nearDist = results[i - 1].distance;
		MathLib::GetPerspFrustumPlanes(
			cameraRight,
			cameraUp,
			cameraForward,
			cameraPosition,
			fov,
			aspect,
			nearDist * (i == 0 ? 1 : tan(fov * 0.5)),
			farDist,
			cameraFrustumPlanes[i].data());
		/*vec4 sphere = GetCascadeSphere(
			cameraPosition,
			rays,
			nearDist,
			result.distance);*/
		vec3 frustumPositions[DIR_COUNT * 2];
		for (auto f : vstd::range(DIR_COUNT)) {
			frustumPositions[f] = cameraPosition + rays[f] * nearDist;
			frustumPositions[f + DIR_COUNT] = cameraPosition + rays[f] * farDist;
		}
		vec3 minPos(1e7f);
		vec3 maxPos(-1e7f);
		float frustumNearDist = nearDist * (i == 0 ? 1 : tan(fov * 0.5));

		for (auto f : vstd::range(DIR_COUNT - 1)) {
			auto pos = cameraPosition + rays[f] * frustumNearDist;
			minPos = min(minPos, pos);
			maxPos = max(maxPos, pos);
			pos = cameraPosition + rays[f] * (farDist / tan(fov * 0.5f));
			minPos = min(minPos, pos);
			maxPos = max(maxPos, pos);
		}
		camFrustumMinMax[i] = {minPos, maxPos};
		auto bbox = GetFrustumLightBBox(
			sunWorldToLocal,
			farDist,
			rays,
			vstd::span<vec3>(frustumPositions, vstd::array_count(frustumPositions)));

		static constexpr bool ENABLE_PIXEL_OFFSET = true;

		if constexpr (ENABLE_PIXEL_OFFSET) {
			vec3 localPosition = bbox.position;
			vec3 positionMovement = localPosition - result.lastPosition;
			float pixelLength = bbox.planeSize / resolution;
			positionMovement = floor(positionMovement / pixelLength + 1e-3f) * pixelLength;
			localPosition = result.lastPosition + positionMovement;
			vec3 position = (vec3)(sunLocalToWorld * vec4(localPosition, 1));
			result.lastPosition = localPosition;
			result.position = position;
		} else {
			result.position = (sunLocalToWorld * vec4(bbox.position, 1));
		}
		result.size = bbox.planeSize * 0.5f;
	}
}
}// namespace toolhub::renderer