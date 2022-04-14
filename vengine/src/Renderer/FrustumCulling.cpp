//#define EXPORT_EXE

#include <Renderer/FrustumCulling.h>
#include <Utility/MathLib.h>
#include <taskflow/taskflow.hpp>
#include <utility/parallel_task.h>
static_assert(sizeof(vec3) == 12 && alignof(vec3) == 4);
static_assert(sizeof(vec4) == 16 && alignof(vec4) == 4);
static_assert(sizeof(vec2) == 8 && alignof(vec4) == 4);
static_assert(sizeof(mat4) == 64 && alignof(mat4) == 4);
static_assert(sizeof(mat3) == 36 && alignof(mat3) == 4);
namespace toolhub::renderer {
mat4 GetCSMTransformMat(const vec3& right, const vec3& up, const vec3& forward, const vec3& position);
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
	vstd::span<FrustumCulling::ShadowmapData> results);
FrustumCulling::FrustumCulling(uint threadCount)
	: executor(threadCount) {
}
FrustumCulling::~FrustumCulling() {
}

void FrustumCulling::Task(size_t i) {
	args[i].multi_visit(
		[&](auto&& v) { executedTask++; CullCSM(v.first, v.second, i); },
		[&](auto&& v) { CullCamera(v); });
}
void FrustumCulling::CullCamera(CameraArgs const& args) {
	//TODO
}
void FrustumCulling::CullCSM(CSMArgs const& args, vstd::span<ShadowmapData> cascades, size_t camCount) {
	auto&& camVec = cullResults[camCount];
	camVec.resize(max(camVec.size(), cascades.size()));
	GetCascadeShadowmapMatrices(
		args.sunRight,
		args.sunUp,
		args.sunForward,
		args.cameraRight,
		args.cameraUp,
		args.cameraForward,
		args.cameraPosition,
		args.fov,
		args.aspect,
		args.zDepth,
		args.resolution,
		args.nearPlane,
		cascades);
	vec3 frustumPoints[8];
	vec4 frustumPlanes[6];
	for (auto i : vstd::range(cascades.size())) {
		auto&& data = cascades[i];
		MathLib::GetOrthoCamFrustumPoints(
			args.sunRight,
			args.sunUp,
			args.sunForward,
			data.position,
			data.size,
			data.size,
			-args.zDepth,
			0,
			frustumPoints);
		vec3 frustumMin(Float32MaxValue);
		vec3 frustumMax(Float32MinValue);
		for (auto&& i : frustumPoints) {
			frustumMin = min(frustumMin, i);
			frustumMax = max(frustumMax, i);
		}
		MathLib::GetOrthoCamFrustumPlanes(
			args.sunRight,
			args.sunUp,
			args.sunForward,
			data.position,
			data.size,
			data.size,
			-args.zDepth,
			0,
			frustumPlanes);
		for (auto idx : vstd::range(transforms.size())) {
			auto&& obj = transforms[idx];
			auto mat = GetCSMTransformMat(
				obj.right,
				obj.up,
				obj.forward,
				obj.position);
			if (MathLib::BoxIntersect(
					mat,
					frustumPlanes,
					obj.bboxCenter,
					obj.bboxExtent,
					frustumMin,
					frustumMax)) {
				camVec[i].emplace_back(idx);
			}
		}
	}
}
static vstd::optional<FrustumCulling> context;
void FrustumCulling::Complete() {
	shadowTask.wait();
}
VENGINE_UNITY_EXTERN void InitFrustumCullContext(uint threadCount) {
	context.New(threadCount);
}
VENGINE_UNITY_EXTERN void DestroyFrustumCullContext() {
	if (context->shadowTask.valid()) {
		context->shadowTask.wait();
	}
	context.Delete();
}
VENGINE_UNITY_EXTERN void AddCullObject(TRS const& data) {
	context->transforms.emplace_back(data);
}
VENGINE_UNITY_EXTERN void UpdateCullObject(TRS const& data, uint index) {
	context->transforms[index] = data;
}
VENGINE_UNITY_EXTERN void RemoveCullObject(uint idx) {
	if (idx < context->transforms.size() - 1)
		context->transforms[idx] = context->transforms.erase_last();
	else
		context->transforms.erase_last();
}
VENGINE_UNITY_EXTERN void ExecuteCull() {
	context->planedTask++;
	tf::Taskflow flow;
	auto beforeTask = flow.emplace([&] {
		context->cullResults.resize(context->args.size());
	});
	auto afterTask = flow.emplace_all(
		[](size_t i) {
			context->Task(i);
		},
		context->args.size(), context->executor.num_workers());
	beforeTask.precede(afterTask);
	context->shadowTask = context->executor.run(std::move(flow));
}
VENGINE_UNITY_EXTERN void CompleteCull() {
	context->Complete();
}
VENGINE_UNITY_EXTERN void ClearCSM() {
	context->args.clear();
}
VENGINE_UNITY_EXTERN uint BindCSM(
	FrustumCulling::CSMArgs& args,
	FrustumCulling::ShadowmapData* cascades,
	uint cascadeCount) {
	auto size = context->args.size();
	context->args.emplace_back(
		args,
		vstd::vector<FrustumCulling::ShadowmapData>(cascades, cascadeCount));
	return size;
}
VENGINE_UNITY_EXTERN void CullingResult(
	uint camIndex,
	uint cascadeIndex,
	FrustumCulling::ShadowmapData& shadowmapData,
	uint*& resultArray,
	uint& resultCount) {
	vstd::span<uint> results(context->cullResults[camIndex][cascadeIndex]);
	resultArray = results.data();
	resultCount = results.size();
	auto&& vec = context->args[camIndex].get<0>().second;
	shadowmapData = vec[cascadeIndex];
}

}// namespace toolhub::renderer
#define EXPORT_EXE
#ifdef EXPORT_EXE
int main() {

	using namespace toolhub::renderer;
	while (true) {
		InitFrustumCullContext(1);
		TRS tt = {
			vec3(0),
			vec3(1, 0, 0),
			vec3(0, 1, 0),
			vec3(0, 0, 1),
			vec3(0),
			vec3(1)};
		AddCullObject(tt);
		vstd::vector<FrustumCulling::ShadowmapData> cascades;
		uint dists[] = {5, 10, 25, 50};
		cascades.push_back_func(
			4,
			[&](size_t i) {
				return dists[i];
			});
		FrustumCulling::CSMArgs csmArg = {
			vec3(1, 0, 0),
			vec3(0, 1, 0),
			vec3(0, 0, 1),
			vec3(1, 0, 0),
			vec3(0, 1, 0),
			vec3(0, 0, 1),
			vec3(0),
			float(60 * Deg2Rad),
			1,
			500,
			2048,
			0.3f};
		UpdateCullObject(tt, 0);
		ClearCSM();
		BindCSM(
			csmArg,
			cascades.data(),
			cascades.size());
		while (true) {
			ExecuteCull();
			CompleteCull();
			uint* arr;
			uint count;
			for (auto d : vstd::range(4)) {
				CullingResult(
					0,
					0,
					cascades[d],
					arr,
					count);
			}
		}

		DestroyFrustumCullContext();
		system("pause");
	}
	return 0;
}
#endif