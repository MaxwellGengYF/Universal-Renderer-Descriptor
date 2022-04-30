#pragma once
#include <glm/Include.h>
#include <taskflow/taskflow.hpp>
using namespace glm;
namespace toolhub::renderer {

struct TRS {
	vec3 position;
	vec3 right;
	vec3 up;
	vec3 forward;
	vec3 bboxCenter;
	vec3 bboxExtent;
};
class FrustumCulling : public vstd::IOperatorNewBase {
public:
	struct ShadowmapData {
		vec3 position;
		vec3 lastPosition;
		float size;
		float distance;
		ShadowmapData(float distance)
			: distance(distance) {
			size = 0;
			position = vec3(0);
			lastPosition = vec3(0);
		}
	};
	struct CSMArgs {
		vec3 cameraRight;
		vec3 cameraUp;
		vec3 cameraForward;
		vec3 cameraPosition;
		float fov;
		float aspect;
		float resolution;
		float nearPlane;
	};
	using CascadeArgs = std::pair<CSMArgs, vstd::vector<ShadowmapData>>;
	struct CameraArgs {
	};
	struct BoxVolume {
		std::array<vec4, 6> planes;
		vec3 minPoint;
		vec3 maxPoint;
	};
	struct Sphere {
		vec3 center;
		float radius;
	};
	struct ProjectBBox {
		vec3 center;
		vec3 extent;
	};
	vec3 sunRight;
	vec3 sunUp;
	vec3 sunForward;
	float zDepth;

	mat4 sunLocalToWorld;
	vstd::vector<TRS> bboxVolumes;
	vstd::vector<size_t> inVolumeObjs;

	tf::Executor executor;
	tf::Future<void> shadowTask;
	vstd::vector<vstd::vector<vstd::vector<uint>>> cullResults;
	vstd::vector<std::pair<TRS, ProjectBBox>> transforms;
	vstd::vector<vstd::variant<
		CascadeArgs,
		CameraArgs>>
		args;
	FrustumCulling(uint threadCount);
	~FrustumCulling();
	void Task(size_t i);
	void Complete();
	void CullCamera(CameraArgs const& args);
	void CullCSM(CSMArgs const& args, vstd::span<ShadowmapData> cascades, size_t camCount);
	void ExecuteCull();
	void CalcVolume();
};
}// namespace toolhub::renderer