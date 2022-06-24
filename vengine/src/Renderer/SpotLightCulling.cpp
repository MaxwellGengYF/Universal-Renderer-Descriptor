#include "SpotLightCulling.h"
#include <Utility/MathLib.h>
namespace toolhub::renderer {
void SpotLightCulling::ExecuteCull() {
	tf::Taskflow flow;
	flow.emplace_all(
		[&](size_t i) {
			std::shared_lock lck(camLock);
			Task(*cullUnits[i]);
		},
		cullUnits.size(),
		executor.num_workers());
	shadowTask = executor.run(std::move(flow));
}
void SpotLightCulling::Wait() {
	if (shadowTask.valid()) shadowTask.wait();
	shadowTask = {};
}
void SpotLightCulling::Task(CullUnit& unit) {
	unit.objectResults.clear();
	unit.objectResults.reserve(objects.posCount);
	auto&& cam = unit.camArgs;
	auto frustumBBox = MathLib::GetFrustumBoundingBox(
		cam.cameraRight,
		cam.cameraUp,
		cam.cameraForward,
		cam.cameraPosition,
		cam.fov,
		cam.aspect,
		cam.nearPlane,
		cam.farPlane);
	std::array<vec4, 6> frustumPlanes;
	MathLib::GetPerspFrustumPlanes(
		cam.cameraRight,
		cam.cameraUp,
		cam.cameraForward,
		cam.cameraPosition,
		cam.fov,
		cam.aspect,
		cam.nearPlane,
		cam.farPlane,
		frustumPlanes.data());
	auto CullObj = [&]<bool retMatrix>(TRS const& tr) {
		mat4 target;
		target[0] = vec4(tr.right, 0);
		target[1] = vec4(tr.up, 0);
		target[2] = vec4(tr.forward, 0);
		target[3] = vec4(tr.position, 1);
		bool contact = MathLib::BoxIntersect(
			target,
			frustumPlanes.data(),
			vec3(0),
			tr.bboxExtent,
			frustumBBox.first,
			frustumBBox.second);
		if constexpr (retMatrix) {
			if (contact)
				return vstd::optional<mat4>(target);
			else
				return vstd::optional<mat4>();
		} else {
			return contact;
		}
	};
	// cull objects
	for (auto i : vstd::range(objects.posCount)) {
		auto&& tr = objects.poses[i];
		if (!CullObj.operator()<false>(tr)) continue;
		unit.objectResults.push_back(i);
	}
	// cull instances
	for (auto instIdx : vstd::range(instances.size())) {
		auto& inst = instances[instIdx];
		auto& count = unit.instCount[instIdx];
		count = 0;
		auto matArr = unit.instMatrices[instIdx];
		for (auto i : vstd::range(inst->posCount)) {
			auto mat = CullObj.operator()<true>(inst->poses[i]);
			if (mat) {
				matArr[count] = *mat;
				++count;
			}
		}
	}
}
SpotLightCulling::SpotLightCulling()
	: executor(std::min<size_t>(std::thread::hardware_concurrency(), 4)),
	  cullUnitPool(8),
	  instancePool(8) {}
SpotLightCulling::~SpotLightCulling() {}
namespace SPC {
vstd::optional<SpotLightCulling> context;
};
VENGINE_UNITY_EXTERN void spcInit() {
	SPC::context.New();
}
VENGINE_UNITY_EXTERN void spcDestroy() {
	SPC::context.Delete();
}
VENGINE_UNITY_EXTERN SpotLightCulling::CullUnit* spcAllocateCamera() {
	return SPC::context->cullUnitPool.New();
}
VENGINE_UNITY_EXTERN void spcDestroyCamera(SpotLightCulling::CullUnit* ptr) {
	SPC::context->cullUnitPool.Delete(ptr);
}
VENGINE_UNITY_EXTERN void spcClearCameraCmd() {
	using namespace SPC;
	std::lock_guard lck(context->camLock);
	context->cullUnits.clear();
}
VENGINE_UNITY_EXTERN void spcAddCameraCmd(SpotLightCulling::CullUnit* ptr) {
	using namespace SPC;
	std::lock_guard lck(context->camLock);
	context->cullUnits.push_back(ptr);
}
VENGINE_UNITY_EXTERN SpotLightCulling::MeshInstance* spcAllocateInstancer() {
	return SPC::context->instancePool.New();
}
VENGINE_UNITY_EXTERN void spcDestroyInstancer(SpotLightCulling::MeshInstance* ptr) {
	SPC::context->instancePool.Delete(ptr);
}
VENGINE_UNITY_EXTERN void spcClearInstanceCmd() {
	using namespace SPC;
	std::lock_guard lck(context->camLock);
	context->instances.clear();
}
VENGINE_UNITY_EXTERN void spcAddInstanceCmd(SpotLightCulling::MeshInstance* ptr) {
	using namespace SPC;
	std::lock_guard lck(context->camLock);
	context->instances.push_back(ptr);
}
VENGINE_UNITY_EXTERN void spcExecute() {
	SPC::context->ExecuteCull();
}
VENGINE_UNITY_EXTERN void spcWait() {
	SPC::context->Wait();
}
}// namespace toolhub::renderer