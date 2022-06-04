
#include <Utility/MathLib.h>
#include <Common/MetaLib.h>
#define GetVec(name, v) vec4 name = XMLoadvec3(&##v);
#define StoreVec(ptr, v) XMStorevec3(ptr, v);
vec4 MathLib::GetPlane(
	const vec3& a,
	const vec3& b,
	const vec3& c) noexcept {
	vec3 normal = normalize(cross(b - a, c - a));
	float disVec = -dot(normal, a);
	return vec4(normal, disVec);
}
float MathLib::GetScreenMultiple(
	float fov,
	float aspect) noexcept {
	float tanFov = tan(0.5f * fov);
	return max(
			   (1 / tanFov),
			   aspect / tanFov)
		   * 0.5f;
}
float MathLib::CalculateTextureMipQFactor(
	float fov,
	float distance,
	uint camRes,
	float texRes,
	float panelSize) noexcept {
	float v = camRes / (distance * tan(fov * 0.5f) * 2);
	float size = (texRes / panelSize) / v;
	return 0.5f * log2(size * size);
}
float MathLib::CalculateMipDistance(float fov, float mip, uint camRes, float texRes, float panelSize) noexcept {
	float r = (texRes / panelSize);
	float rDivideV = sqrt(pow(2, mip * 2));
	float v = r / rDivideV;
	v = camRes / v;
	v /= (tan(fov * 0.5f) * 2);
	return v;
}
float MathLib::GetScreenPercentage(mat4 const& projectMat,
								   float objectToCameraDistance,
								   float sphereRadius) {
	float screenMultiple = max(0.5f * projectMat[0].x, 0.5f * projectMat[1].y);
	float screenRadius = screenMultiple * sphereRadius / max(1.0f, objectToCameraDistance);
	return screenRadius * 2;
}
float MathLib::GetHalton_Float(uint index) {
	index = (index << 16) | (index >> 16);
	index = ((index & 0x00ff00ff) << 8) | ((index & 0xff00ff00) >> 8);
	index = ((index & 0x0f0f0f0f) << 4) | ((index & 0xf0f0f0f0) >> 4);
	index = ((index & 0x33333333) << 2) | ((index & 0xcccccccc) >> 2);
	index = ((index & 0x55555555) << 1) | ((index & 0xaaaaaaaa) >> 1);
	uint u = 0x3f800000u | (index >> 9);
	return (float)(((double&)u) - 1);
}
float MathLib::GetScreenPercentage(
	vec3 const& boundsOrigin,
	vec3 const& camOrigin,
	float screenMultiple,
	float sphereRadius) noexcept {
	float dist = distance(boundsOrigin, camOrigin);
	float screenRadius = screenMultiple * sphereRadius / max(1.0f, dist);
	return screenRadius;
}
vec4 MathLib::GetPlane(
	const vec3& normal,
	const vec3& inPoint) noexcept {
	float dt = -dot(normal, inPoint);
	vec4 result(normal, dt);
	return result;
}
template<bool toInside>
static bool MathLib::mBoxIntersect(
	const mat4& localToWorldmat, vec4* planes, const vec3& position, const vec3& localExtent,
	const vec3& frustumminPoint, const vec3& frustummaxPoint) noexcept {
	static const vec3 arrays[8] =
		{
			vec3(1, 1, 1),
			vec3(1, -1, 1),
			vec3(1, 1, -1),
			vec3(1, -1, -1),
			vec3(-1, 1, 1),
			vec3(-1, -1, 1),
			vec3(-1, 1, -1),
			vec3(-1, -1, -1)};
	vec4 minPos = localToWorldmat * vec4(position + arrays[0] * localExtent, 1);
	vec4 maxPos = minPos;
	for (uint i = 1; i < 8; ++i) {
		vec4 worldPos = vec4(position + arrays[i] * localExtent, 1);
		worldPos = localToWorldmat * worldPos;
		minPos = min(minPos, worldPos);
		maxPos = max(maxPos, worldPos);
	}
	auto IteVec = [&](bool a, auto&& compareFunc, auto&& func, auto&& vec, auto&& vec1) {
		a = compareFunc(a, func(vec.x, vec1.x));
		a = compareFunc(a, func(vec.y, vec1.y));
		a = compareFunc(a, func(vec.z, vec1.z));
		return a;
	};

	if constexpr (toInside) {
		if (IteVec(false, Or, Less<float>, minPos, frustumminPoint)
			|| IteVec(false, Or, Greater<float>, maxPos, frustummaxPoint))
			return false;
	} else {
		if (IteVec(false, Or, Greater<float>, minPos, frustummaxPoint)
			|| IteVec(false, Or, Less<float>, maxPos, frustumminPoint))
			return false;
	}
	vec3 pos = static_cast<vec3>(localToWorldmat * vec4(position, 1));
	mat3 normalLocalMat = transpose((mat3)localToWorldmat);
	for (uint i = 0; i < 6; ++i) {
		vec4 plane = planes[i];
		vec3 const& planeXYZ = reinterpret_cast<vec3 const&>(plane);
		vec3 absNormal = abs(normalLocalMat * planeXYZ);
		float result = dot(pos, planeXYZ) - dot(absNormal, localExtent);
		if constexpr (toInside) {
			if (result < -plane.w) return false;
		} else {
			if (result > -plane.w) return false;
		}
	}
	return true;
}
bool MathLib::BoxIntersect(const mat4& localToWorldmat, vec4* planes, const vec3& position, const vec3& localExtent,
						   const vec3& frustumminPoint, const vec3& frustummaxPoint) noexcept {
	return mBoxIntersect<false>(localToWorldmat, planes, position, localExtent, frustumminPoint, frustummaxPoint);
}
bool MathLib::InnerBoxIntersect(const mat4& localToWorldmat, vec4* planes, const vec3& position, const vec3& localExtent,
								const vec3& frustumminPoint, const vec3& frustummaxPoint) noexcept {
	return mBoxIntersect<true>(localToWorldmat, planes, position, localExtent, frustumminPoint, frustummaxPoint);
}
void MathLib::GetCameraNearPlanePoints(
	const vec3& right,
	const vec3& up,
	const vec3& forward,
	const vec3& position,
	float fov,
	float aspect,
	float distance,
	vec3* corners) noexcept {
	float upLength = distance * tan(fov * 0.5);
	float rightLength = upLength * aspect;
	vec3 farPoint = position + distance * forward;
	vec3 upVec = upLength * up;
	vec3 rightVec = rightLength * right;
	corners[0] = farPoint - upVec - rightVec;
	corners[1] = farPoint - upVec + rightVec;
	corners[2] = farPoint + upVec - rightVec;
	corners[3] = farPoint + upVec + rightVec;
}
void MathLib::GetPerspFrustumPlanes(
	const vec3& right,
	const vec3& up,
	const vec3& forward,
	const vec3& position,
	float fov,
	float aspect,
	float nearPlane,
	float farPlane,
	vec4* frustumPlanes) noexcept {
	vec3 nearCorners[4];
	GetCameraNearPlanePoints(right, up, forward, position, fov, aspect, nearPlane, nearCorners);
	frustumPlanes[0] = GetPlane(forward, (position + farPlane * forward));
	frustumPlanes[1] = GetPlane(-forward, (position + nearPlane * forward));
	frustumPlanes[2] = GetPlane((nearCorners[1]), (nearCorners[0]), (position));
	frustumPlanes[3] = GetPlane((nearCorners[2]), (nearCorners[3]), (position));
	frustumPlanes[4] = GetPlane((nearCorners[0]), (nearCorners[2]), (position));
	frustumPlanes[5] = GetPlane((nearCorners[3]), (nearCorners[1]), (position));
}
std::pair<vec3, vec3> MathLib::GetFrustumBoundingBox(
	const vec3& right,
	const vec3& up,
	const vec3& forward,
	const vec3& pos,
	float fov,
	float aspect,
	float nearZ,
	float farZ) noexcept {
	float tt = tan(fov * 0.5);
	float halfNearYHeight = nearZ * tt;
	float halfFarYHeight =  farZ * tt;
	float halfNearXWidth = halfNearYHeight * aspect;
	float halfFarXWidth = halfFarYHeight * aspect;
	vec3 poses[8];
	poses[0] = pos + forward * nearZ - right * halfNearXWidth - up * halfNearYHeight;
	poses[1] = pos + forward * nearZ - right * halfNearXWidth + up * halfNearYHeight;
	poses[2] = pos + forward * nearZ + right * halfNearXWidth - up * halfNearYHeight;
	poses[3] = pos + forward * nearZ + right * halfNearXWidth + up * halfNearYHeight;
	poses[4] = pos + forward * farZ - right * halfFarXWidth - up * halfFarYHeight;
	poses[5] = pos + forward * farZ - right * halfFarXWidth + up * halfFarYHeight;
	poses[6] = pos + forward * farZ + right * halfFarXWidth - up * halfFarYHeight;
	poses[7] = pos + forward * farZ + right * halfFarXWidth + up * halfFarYHeight;
	vec3 minValue = poses[7];
	vec3 maxValue = minValue;
	for (uint i = 0; i < 7; ++i) {
		minValue = min(poses[i], minValue);
		maxValue = max(poses[i], maxValue);
	};
	return {minValue, maxValue};
}
bool MathLib::ConeIntersect(const Cone& cone, const vec4& plane) noexcept {
	vec3 dir = cone.direction;
	vec3 vertex = cone.vertex;
	vec3 m = cross(cross(reinterpret_cast<vec3 const&>(plane), dir), dir);
	vec3 Q = vertex + dir * cone.height + normalize(m) * cone.radius;
	return (GetPointDistanceToPlane(plane, vertex) < 0) || (GetPointDistanceToPlane(plane, Q) < 0);
}
void MathLib::GetOrthoCamFrustumPlanes(
	const vec3& right,
	const vec3& up,
	const vec3& forward,
	const vec3& position,
	float xSize,
	float ySize,
	float nearPlane,
	float farPlane,
	vec4* results) noexcept {
	vec3 normals[6];
	vec3 positions[6];
	normals[0] = up;
	positions[0] = position + up * ySize;
	normals[1] = -up;
	positions[1] = position - up * ySize;
	normals[2] = right;
	positions[2] = position + right * xSize;
	normals[3] = -right;
	positions[3] = position - right * xSize;
	normals[4] = forward;
	positions[4] = position + forward * farPlane;
	normals[5] = -forward;
	positions[5] = position + forward * nearPlane;
	for (uint i = 0; i < 6; ++i) {
		results[i] = GetPlane(normals[i], positions[i]);
	};
}
void MathLib::GetOrthoCamFrustumPoints(
	const vec3& right,
	const vec3& up,
	const vec3& forward,
	const vec3& position,
	float xSize,
	float ySize,
	float nearPlane,
	float farPlane,
	vec3* results) noexcept {
	results[0] = position + xSize * right + ySize * up + farPlane * forward;
	results[1] = position + xSize * right + ySize * up + nearPlane * forward;
	results[2] = position + xSize * right - ySize * up + farPlane * forward;
	results[3] = position + xSize * right - ySize * up + nearPlane * forward;
	results[4] = position - xSize * right + ySize * up + farPlane * forward;
	results[5] = position - xSize * right + ySize * up + nearPlane * forward;
	results[6] = position - xSize * right - ySize * up + farPlane * forward;
	results[7] = position - xSize * right - ySize * up + nearPlane * forward;
}
float MathLib::DistanceToCube(const vec3& cubeSize, const vec3& relativePosToCube) noexcept {
	vec3 absPos = abs(relativePosToCube);
	vec4 plane = GetPlane(vec3(0, 1, 0), vec3(0, cubeSize.y, 0));
	float dist = GetPointDistanceToPlane(plane, absPos);
	plane = GetPlane(vec3(1, 0, 0), vec3(cubeSize.x, 0, 0));
	dist = max(dist, GetPointDistanceToPlane(plane, absPos));
	plane = GetPlane(vec3(0, 0, 1), vec3(0, 0, cubeSize.z));
	dist = max(dist, GetPointDistanceToPlane(plane, absPos));
	return dist;
}
float MathLib::DistanceToQuad(float size, vec2 quadToTarget) noexcept {
	vec3 quadVec = vec3(quadToTarget.x, quadToTarget.y, 0);
	quadVec = abs(quadVec);
	float len = length(quadVec);
	quadVec /= len;
	float dotV = max(dot(vec3(0, 1, 0), quadVec), dot(vec3(1, 0, 0), quadVec));
	float leftLen = size / dotV;
	return len - leftLen;
};
bool MathLib::BoxIntersect(const vec3& position, const vec3& extent, vec4* planes) noexcept {
	for (uint i = 0; i < 6; ++i) {
		vec4& plane = planes[i];
		vec3 const& planeXYZ = reinterpret_cast<vec3 const&>(plane);
		vec3 absNormal = abs(planeXYZ);
		if ((dot(position, planeXYZ) - dot(absNormal, extent)) > -(float)plane.w)
			return false;
	}
	return true;
}
bool MathLib::BoxIntersect(
	const vec3& position,
	const vec3& extent,
	vec4* planes,
	const vec3& frustumminPoint,
	const vec3& frustummaxPoint) noexcept {
	vec3 minPos = position - extent;
	vec3 maxPos = position + extent;
	auto IteVec = [&](bool a, auto&& compareFunc, auto&& func, auto&& vec, auto&& vec1) {
		a = compareFunc(a, func(vec.x, vec1.x));
		a = compareFunc(a, func(vec.y, vec1.y));
		a = compareFunc(a, func(vec.z, vec1.z));
		return a;
	};
	if (IteVec(false, Or, Greater<float>, minPos, frustummaxPoint)
		|| IteVec(false, Or, Less<float>, maxPos, frustumminPoint))
		return false;
	for (uint i = 0; i < 6; ++i) {
		vec4& plane = planes[i];
		vec3 const& planeXYZ = reinterpret_cast<vec3 const&>(plane);
		vec3 absNormal = abs(planeXYZ);
		if ((dot(position, planeXYZ) - dot(absNormal, extent)) > -(float)plane.w)
			return false;
	}
	return true;
}
vec4 MathLib::CameraSpacePlane(const mat4& worldToCameramat, const vec3& pos, const vec3& normal, float clipPlaneOffset) noexcept {
	vec4 offsetPos = vec4(pos + normal * clipPlaneOffset, 1);
	vec4 cpos = worldToCameramat * offsetPos;
	cpos.w = 0;

	vec4 cnormal = normalize(worldToCameramat * vec4(normal, 0));
	cnormal.w = (-dot(cpos, cnormal));
	return cnormal;
}
