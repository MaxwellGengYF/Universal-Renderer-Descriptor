#pragma once
#include <Renderer/Camera.h>
#include <taskflow/taskflow.hpp>
#include <shared_mutex>
namespace toolhub::renderer {

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
	struct CamArgs : public CSMArgs {
		float farPlane;
	};
	using CascadeArgs = std::pair<CSMArgs, vstd::vector<ShadowmapData>>;
	struct BoxVolume {
		std::array<vec4, 6> planes;
		vec3 minPoint;
		vec3 maxPoint;
	};
	struct Sphere {
		vec3 center;
		float radius;
	};

	/*struct MeshInstance {
		vstd::span<std::pair<TRS, ProjectBBox>> transforms;
		mat4* ptr;
		size_t resultCount;
	};*/
	vec3 sunRight;
	vec3 sunUp;
	vec3 sunForward;
	float zDepth;

	mat4 sunLocalToWorld;
	vstd::vector<TRS> bboxVolumes;
	//vstd::vector<size_t> inVolumeObjs;
	std::shared_mutex mtx;
	tf::Executor executor;
	tf::Future<void> shadowTask;
	vstd::vector<vstd::vector<vstd::vector<uint>>> shaodwCullResults;
	vstd::vector<vstd::vector<uint>> camCullResults;
	vstd::vector<std::pair<TRS, ProjectBBox>> transforms;

	//vstd::vector<MeshInstance> instances;

	vstd::vector<CascadeArgs> shadowArgs;
	vstd::vector<CamArgs> camArgs;
	FrustumCulling(uint threadCount);
	~FrustumCulling();
	void ShadowTask(size_t i);
	void CamTask(size_t i);
	void Complete();
	void CullCSM(CSMArgs const& args, vstd::span<ShadowmapData> cascades, size_t camCount);
	void ExecuteCull();
	void CalcVolume();
};
}// namespace toolhub::renderer