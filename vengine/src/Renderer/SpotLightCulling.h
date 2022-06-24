#pragma once
#include <Renderer/Camera.h>
#include <taskflow/taskflow.hpp>
#include <shared_mutex>
namespace toolhub::renderer {
class SpotLightCulling : public vstd::IOperatorNewBase {
public:
	struct CullUnit {
		CamArgs camArgs;
		mat4** instMatrices;
		uint* instCount;
		vstd::vector<uint> objectResults;
	};
	std::shared_mutex camLock;
	struct MeshInstance {
		TRS* poses;
		size_t posCount;
	};
	vstd::Pool<CullUnit> cullUnitPool;
	vstd::Pool<MeshInstance> instancePool;
    vstd::vector<CullUnit*> cullUnits;
	vstd::vector<MeshInstance*> instances;

	MeshInstance objects;
	tf::Executor executor;
	tf::Future<void> shadowTask;
	SpotLightCulling();
	~SpotLightCulling();
	void ExecuteCull();
	void Wait();

private:
	void Task(CullUnit& unit);
};
}// namespace toolhub::renderer