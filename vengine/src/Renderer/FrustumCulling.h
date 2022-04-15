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
		vec3 sunRight;
		vec3 sunUp;
		vec3 sunForward;
		vec3 cameraRight;
		vec3 cameraUp;
		vec3 cameraForward;
		vec3 cameraPosition;
		float fov;
		float aspect;
		float zDepth;
		float resolution;
		float nearPlane;
	};
	using CascadeArgs = std::pair<CSMArgs, vstd::vector<ShadowmapData>>;
	struct CameraArgs {
	};
	tf::Executor executor;
	tf::Future<void> shadowTask;
	vstd::vector<vstd::vector<vstd::vector<uint>>> cullResults;
	vstd::vector<TRS> transforms;
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
};
}// namespace toolhub::renderer