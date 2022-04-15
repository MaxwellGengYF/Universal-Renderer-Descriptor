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
static constexpr size_t DIR_COUNT = 4;
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
		normalize(forward + upDir + rightDir)};
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
	vec3 const& sunRight,
	vec3 const& sunUp,
	vec3 const& sunForward,
	vec3 const& cameraRight,
	vec3 const& cameraUp,
	vec3 const& cameraForward,
	vec3 const& cameraPosition,
	float fov,
	float aspect,
	float zDepth,
	float resolution,
	float nearPlane,
	vstd::span<FrustumCulling::ShadowmapData> results) {
	mat4 sunLocalToWorld = GetCSMTransformMat(
		sunRight,
		sunUp,
		sunForward,
		vec3(0, 0, 0));
	mat4 sunWorldToLocal = inverse(sunLocalToWorld);
	auto rays = GetFrustumRay(cameraRight, cameraUp, cameraForward, fov, aspect);
	for (auto i : vstd::range(results.size())) {
		auto&& result = results[i];
		float nearDist;
		if (i == 0)
			nearDist = nearPlane;
		else
			nearDist = results[i - 1].distance;
		vec4 sphere = GetCascadeSphere(
			cameraPosition,
			rays,
			nearDist,
			result.distance);
		static constexpr bool ENABLE_PIXEL_OFFSET = true;

		if constexpr (ENABLE_PIXEL_OFFSET) {
			vec3 localPosition = sunWorldToLocal * vec4(vec3(sphere), 1);
			vec3 positionMovement = localPosition - result.lastPosition;
			float pixelLength = (sphere.w * 2) / resolution;
			positionMovement = floor(positionMovement / pixelLength) * pixelLength;
			localPosition = result.lastPosition + positionMovement;
			vec3 position = (vec3)(sunLocalToWorld * vec4(localPosition, 1));
			result.lastPosition = localPosition;
			result.position = position;
		} else {
			result.position = sphere;
		}
		result.position += sphere.w * sunForward;
		result.size = sphere.w;
	}
}
}// namespace toolhub::renderer