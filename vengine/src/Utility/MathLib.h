#pragma once
#include <VEngineConfig.h>
#include <Common/Common.h>
#include <Utility/MathDefine.h>
#include <glm/Include.h>
using namespace glm;
struct Cone {
	vec3 vertex;
	float height;
	vec3 direction;
	float radius;
	Cone(const vec3& position, float distance, const vec3& direction, float angle) noexcept
		: vertex(position),
		  height(distance),
		  direction(direction) {
		radius = tan(angle * 0.5 * Deg2Rad) * height;
	}
};
class VENGINE_DLL_COMMON MathLib final {
	template<bool toInside>
	static bool mBoxIntersect(
		const mat4& localToWorldMatrix,
		vec4* planes,
		const vec3& localPosition,
		const vec3& localExtent,
		const vec3& frustumMinPoint,
		const vec3& frustumMaxPoint) noexcept;

public:
	MathLib() = delete;
	~MathLib() = delete;
	static bool And(bool a, bool b) { return a && b; }
	static bool Or(bool a, bool b) { return a || b; }
	template<typename T>
	static bool Greater(T const& a, T const& b) { return a > b; }
	template<typename T>
	static bool GEqual(T const& a, T const& b) { return a >= b; }
	template<typename T>
	static bool Less(T const& a, T const& b) { return a < b; }
	template<typename T>
	static bool LEqual(T const& a, T const& b) { return a <= b; }
	template<typename T>
	static bool Equal(T const& a, T const& b) { return a == b; }
	static vec4 GetPlane(
		const vec3& normal,
		const vec3& inPoint) noexcept;
	static vec4 GetPlane(
		const vec3& a,
		const vec3& b,
		const vec3& c) noexcept;
	/*static bool BoxIntersect(
		const mat4& localToWorldMatrix,
		vec4* planes,
		const vec3& localPosition,
		const vec3& localExtent);*/
	static bool BoxIntersect(
		const mat4& localToWorldMatrix,
		vec4* planes,
		const vec3& localPosition,
		const vec3& localExtent,
		const vec3& frustumMinPoint,
		const vec3& frustumMaxPoint) noexcept;
	static bool InnerBoxIntersect(
		const mat4& localToWorldMatrix,
		vec4* planes,
		const vec3& localPosition,
		const vec3& localExtent,
		const vec3& frustumMinPoint,
		const vec3& frustumMaxPoint) noexcept;
	/*static void GetCameraNearPlanePoints(
		const mat4& localToWorldMatrix,
		float fov,
		float aspect,
		float distance,
		vec3* corners
	);*/

	static void GetCameraNearPlanePoints(
		const vec3& right,
		const vec3& up,
		const vec3& forward,
		const vec3& position,
		float fov,
		float aspect,
		float distance,
		vec3* corners) noexcept;
	/*
	static void GetPerspFrustumPlanes(
		const mat4& localToWorldMatrix,
		float fov,
		float aspect,
		float nearPlane,
		float farPlane,
		float4* frustumPlanes);*/
	static void GetPerspFrustumPlanes(
		const vec3& right,
		const vec3& up,
		const vec3& forward,
		const vec3& position,
		float fov,
		float aspect,
		float nearPlane,
		float farPlane,
		vec4* frustumPlanes) noexcept;
	static float GetScreenMultiple(
		float fov,
		float aspect) noexcept;
	static float GetScreenPercentage(
		vec3 const& boundsOrigin,
		vec3 const& camOrigin,
		float screenMultiple,
		float sphereRadius) noexcept;
	/*static void GetPerspFrustumPlanes(
		const mat4& localToWorldMatrix,
		float fov,
		float aspect,
		float nearPlane,
		float farPlane,
		vec4* frustumPlanes
	);

	static void GetFrustumBoundingBox(
		const mat4& localToWorldMatrix,
		float nearWindowHeight,
		float farWindowHeight,
		float aspect,
		float nearZ,
		float farZ,
		vec3* minValue,
		vec3* maxValue
	);*/
	static std::pair<vec3, vec3> GetFrustumBoundingBox(
		const vec3& right,
		const vec3& up,
		const vec3& forward,
		const vec3& position,
		float fov,
		float aspect,
		float nearZ,
		float farZ) noexcept;

	static mat4 GetOrthoMatrix(
		float nearPlane,
		float farPlane,
		float size,
		float aspect,
		bool renderTarget,
		bool gpuResource) noexcept;

	static float GetPointDistanceToPlane(
		const vec4& plane,
		const vec3& point) noexcept {
		float dotValue = dot((vec3)plane, (vec3)point);
		return dotValue + plane.w;
	}
	static bool ConeIntersect(const Cone& cone, const vec4& plane) noexcept;
	static void GetOrthoCamFrustumPlanes(
		const vec3& right,
		const vec3& up,
		const vec3& forward,
		const vec3& position,
		float xSize,
		float ySize,
		float nearPlane,
		float farPlane,
		vec4* results) noexcept;
	static void GetOrthoCamFrustumPoints(
		const vec3& right,
		const vec3& up,
		const vec3& forward,
		const vec3& position,
		float xSize,
		float ySize,
		float nearPlane,
		float farPlane,
		vec3* results) noexcept;

	static float DistanceToCube(const vec3& cubeSize, const vec3& relativePosToCube) noexcept;
	static float DistanceToQuad(float size, vec2 quadToTarget) noexcept;
	static bool BoxIntersect(const vec3& position, const vec3& extent, vec4* planes) noexcept;
	static bool BoxIntersect(const vec3& position,
							 const vec3& extent,
							 vec4* planes,
							 const vec3& frustumMinPoint,
							 const vec3& frustumMaxPoint) noexcept;
	static vec4 CameraSpacePlane(const mat4& worldToCameraMatrix, const vec3& pos, const vec3& normal, float clipPlaneOffset) noexcept;
	static float CalculateTextureMipQFactor(
		float fov,
		float distance,
		uint camRes,
		float texRes,
		float panelSize) noexcept;
	static float CalculateMipDistance(
		float fov,
		float mip,
		uint camRes,
		float texRes,
		float panelSize) noexcept;
	static float GetScreenPercentage(
		mat4 const& projectMat,
		float objectToCameraDistance,
		float sphereRadius);
	static float GetHalton_Float(uint index);
	static uint64 CalcAlign(uint64 value, uint64 align) {
		return (value + (align - 1)) & ~(align - 1);
	}
};