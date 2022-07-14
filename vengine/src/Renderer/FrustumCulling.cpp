//#define EXPORT_EXE

#include <Renderer/FrustumCulling.h>
#include <Utility/MathLib.h>
#include <taskflow/taskflow.hpp>
static_assert(sizeof(vec3) == 12 && alignof(vec3) == 4);
static_assert(sizeof(vec4) == 16 && alignof(vec4) == 4);
static_assert(sizeof(vec2) == 8 && alignof(vec4) == 4);
static_assert(sizeof(mat4) == 64 && alignof(mat4) == 4);
static_assert(sizeof(mat3) == 36 && alignof(mat3) == 4);
namespace toolhub::renderer {
mat4 GetCSMTransformMat(const vec3& right, const vec3& up, const vec3& forward, const vec3& position);
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
	std::pair<vec3, vec3>* camFrustumMinMax);
FrustumCulling::FrustumCulling(uint threadCount)
	: executor(threadCount) {
}
FrustumCulling::~FrustumCulling() {
}

void FrustumCulling::CamTask(size_t i) {
	//TODO
}
void FrustumCulling::ShadowTask(size_t i) {
	auto&& v = shadowArgs[i];
	std::shared_lock lck(mtx);
	CullCSM(v.first, v.second, i);
}
void FrustumCulling::CullCSM(CSMArgs const& args, vstd::span<ShadowmapData> cascades, size_t camCount) {
	auto&& camVec = shaodwCullResults[camCount];
	camVec.resize(max(camVec.size(), cascades.size()));
	vstd::vector<std::pair<vec3, vec3>> camFrustumMinMax(cascades.size());
	vstd::vector<std::array<vec4, 6>> camFrustumPlanes(cascades.size());
	static_assert(sizeof(decltype(camFrustumPlanes)::value_type) == sizeof(vec4) * 6);
	GetCascadeShadowmapMatrices(
		sunLocalToWorld,
		args.cameraRight,
		args.cameraUp,
		args.cameraForward,
		args.cameraPosition,
		args.fov,
		args.aspect,
		args.resolution,
		args.nearPlane,
		cascades,
		camFrustumPlanes.data(),
		camFrustumMinMax.data());

	vec3 frustumPoints[8];
	vec4 frustumPlanes[6];
	for (auto i : vstd::range(cascades.size())) {
		auto&& data = cascades[i];
		MathLib::GetOrthoCamFrustumPoints(
			sunRight,
			sunUp,
			sunForward,
			data.position,
			data.size,
			data.size,
			-zDepth,
			0,
			frustumPoints);
		vec3 frustumMin(Float32MaxValue);
		vec3 frustumMax(Float32MinValue);
		for (auto&& i : frustumPoints) {
			frustumMin = min(frustumMin, i);
			frustumMax = max(frustumMax, i);
		}
		camVec[i].clear();

		MathLib::GetOrthoCamFrustumPlanes(
			sunRight,
			sunUp,
			sunForward,
			data.position,
			data.size,
			data.size,
			-zDepth,
			0,
			frustumPlanes);
		for (auto idx : vstd::range(transforms.size())) {
			auto&& obj = transforms[idx].first;
			auto&& projBBox = transforms[idx].second;
			auto&& minMax = camFrustumMinMax[i];
			auto mat = GetCSMTransformMat(
				obj.right,
				obj.up,
				obj.forward,
				obj.position);

			if (!MathLib::BoxIntersect(
					mat,
					frustumPlanes,
					obj.bboxCenter,
					obj.bboxExtent,
					frustumMin,
					frustumMax)) continue;
			//TODO
			if (!MathLib::BoxIntersect(
					sunLocalToWorld,
					camFrustumPlanes[i].data(),
					projBBox.center,
					projBBox.extent,
					minMax.first,
					minMax.second)) continue;
			camVec[i].emplace_back(idx);
		}
	}
}
void FrustumCulling::CalcVolume() {
	vstd::vector<BoxVolume> boxPlanes;
	boxPlanes.push_back_func(
		bboxVolumes.size(),
		[&](size_t i) {
			BoxVolume vol;
			auto&& v = bboxVolumes[i];
			MathLib::GetOrthoCamFrustumPlanes(
				v.right,
				v.up,
				v.forward,
				v.position,
				v.bboxExtent.x,
				v.bboxExtent.y,
				-v.bboxExtent.z,
				v.bboxExtent.z,
				vol.planes.data());
			for (auto&& p : vol.planes) {
				p *= -1;
			}
			vol.minPoint = vec3(1e7f);
			vol.maxPoint = vec3(-1e7f);
			for (auto x : vstd::range(2))
				for (auto y : vstd::range(2))
					for (auto z : vstd::range(2)) {
						vec3 pos(v.position + v.right * (x - 0.5f) * 2.0f * v.bboxExtent.x + v.up * (y - 0.5f) * 2.0f * v.bboxExtent.y + v.forward * (z - 0.5f) * 2.0f * v.bboxExtent.z);
						vol.minPoint = min(vol.minPoint, pos);
						vol.maxPoint = max(vol.maxPoint, pos);
					}
			return vol;
		});
	mat4 sunWorldToLocal = inverse(sunLocalToWorld);
	/*
	for (auto i : vstd::range(transforms.size())) {
		[&] {
			auto&& obj = transforms[i].first;
			auto&& bbox = transforms[i].second;
			auto mat = GetCSMTransformMat(
				obj.right,
				obj.up,
				obj.forward,
				obj.position);
			for (auto&& box : boxPlanes) {
				if (MathLib::InnerBoxIntersect(
						mat,
						box.planes.data(),
						obj.bboxCenter,
						obj.bboxExtent,
						box.minPoint,
						box.maxPoint)) {
					return;
				}
			}
			vec3 objWorldPos(mat * vec4(obj.bboxCenter, 1));
			vec3 objLocalMin(1e7f);
			vec3 objLocalMax(-1e7f);
			for (auto x : vstd::range(0, 2))
				for (auto y : vstd::range(0, 2))
					for (auto z : vstd::range(0, 2)) {
						vec3 offsets(x - 0.5f, y - 0.5f, z - 0.5f);
						offsets *= 2;
						offsets *= obj.bboxExtent;
						vec3 objVertexWorldPos = objWorldPos + offsets.x * obj.right + offsets.y * obj.up + offsets.z * obj.forward;
						vec3 objVertexLocalPos = sunWorldToLocal * vec4(objVertexWorldPos, 1);
						objLocalMin = min(objLocalMin, objVertexLocalPos);
						objLocalMax = max(objLocalMax, objVertexLocalPos);
					}
			bbox.center = mix(objLocalMin, objLocalMax, 0.5f);
			bbox.extent = (objLocalMax - objLocalMin) * 0.5f;
			bbox.center.z += zDepth * 0.5f - bbox.extent.z;
			bbox.extent.z = zDepth * 0.5f;
			inVolumeObjs.emplace_back(i);
		}();
	}*/
}

void FrustumCulling::ExecuteCull() {
	tf::Taskflow flow;
	if (!shadowArgs.empty()) {
		flow.emplace_all(
			[&] {
				shaodwCullResults.resize(shadowArgs.size());
				sunLocalToWorld = GetCSMTransformMat(
					sunRight,
					sunUp,
					sunForward,
					vec3(0));
				CalcVolume();
			},
			[&](size_t i) {
				ShadowTask(i);
			},
			shadowArgs.size(), executor.num_workers());
	}
	if (!camArgs.empty()) {
		flow.emplace_all(
			[&] {
				camCullResults.resize(camArgs.size());
			},
			[&](size_t i) {
				CamTask(i);
			},
			camArgs.size(), executor.num_workers());
	}
	shadowTask = executor.run(std::move(flow));
}
namespace frustum_culling {
static vstd::optional<FrustumCulling> context;
}
void FrustumCulling::Complete() {
	shadowTask.wait();
}
VENGINE_UNITY_EXTERN void InitFrustumCullContext(uint threadCount) {
	frustum_culling::context.New(threadCount);
}
VENGINE_UNITY_EXTERN void DestroyFrustumCullContext() {
	if (frustum_culling::context->shadowTask.valid()) {
		frustum_culling::context->shadowTask.wait();
	}
	frustum_culling::context.Delete();
}
VENGINE_UNITY_EXTERN void AddCullObject(TRS const& data) {
	std::lock_guard lck(frustum_culling::context->mtx);
	frustum_culling::context->transforms.emplace_back(data, ProjectBBox{});
}
VENGINE_UNITY_EXTERN void UpdateCullObject(TRS const& data, uint index) {
	frustum_culling::context->transforms[index].first = data;
}
VENGINE_UNITY_EXTERN void RemoveCullObject(uint idx) {
	std::lock_guard lck(frustum_culling::context->mtx);
	if (idx < frustum_culling::context->transforms.size() - 1)
		frustum_culling::context->transforms[idx] = frustum_culling::context->transforms.erase_last();
	else
		frustum_culling::context->transforms.erase_last();
}
/*
VENGINE_UNITY_EXTERN uint64 AddCullInstance(
	std::pair<TRS, ProjectBBox>* instanceTransforms,
	size_t transformCount,
	mat4* matArr) {
	FrustumCulling::MeshInstance inst;
	inst.transforms = {instanceTransforms, transformCount};
	inst.ptr = matArr;
	inst.resultCount = 0;
	std::lock_guard lck(frustum_culling::context->mtx);
	auto sz = frustum_culling::context->instances.size();
	frustum_culling::context->instances.push_back(inst);
	return sz;
}
VENGINE_UNITY_EXTERN void UpdateCullInstance(
	size_t index,
	std::pair<TRS, ProjectBBox>* instanceTransforms,
	size_t transformCount,
	mat4* matArr) {
	FrustumCulling::MeshInstance inst;
	inst.transforms = {instanceTransforms, transformCount};
	inst.ptr = matArr;
	inst.resultCount = 0;
	std::lock_guard lck(frustum_culling::context->mtx);
	frustum_culling::context->instances[index] = inst;
}
VENGINE_UNITY_EXTERN void RemoveCullInstance(
	size_t idx) {
	std::lock_guard lck(frustum_culling::context->mtx);
	if (idx < frustum_culling::context->instances.size() - 1)
		frustum_culling::context->instances[idx] = frustum_culling::context->instances.erase_last();
	else
		frustum_culling::context->instances.erase_last();
}*/
VENGINE_UNITY_EXTERN void ExecuteCull() {
	frustum_culling::context->ExecuteCull();
}
VENGINE_UNITY_EXTERN void CompleteCull() {
	frustum_culling::context->Complete();
}
VENGINE_UNITY_EXTERN void ClearCSM() {
	frustum_culling::context->shadowArgs.clear();
}
VENGINE_UNITY_EXTERN uint BindCSM(
	FrustumCulling::CSMArgs& args,
	FrustumCulling::ShadowmapData* cascades,
	uint cascadeCount) {
	auto size = frustum_culling::context->shadowArgs.size();
	frustum_culling::context->shadowArgs.emplace_back(
		args,
		vstd::span<FrustumCulling::ShadowmapData>(cascades, cascadeCount));
	return size;
}
VENGINE_UNITY_EXTERN void BindSunLight(
	vec3 sunRight,
	vec3 sunUp,
	vec3 sunForward,
	float zDepth) {
	frustum_culling::context->sunRight = sunRight;
	frustum_culling::context->sunUp = sunUp;
	frustum_culling::context->sunForward = sunForward;
	frustum_culling::context->zDepth = zDepth;
}
VENGINE_UNITY_EXTERN void CullingResult(
	uint camIndex,
	uint cascadeIndex,
	FrustumCulling::ShadowmapData& shadowmapData,
	uint*& resultArray,
	uint& resultCount) {
	vstd::span<uint> results(frustum_culling::context->shaodwCullResults[camIndex][cascadeIndex]);
	resultArray = results.data();
	resultCount = results.size();
	auto&& vec = frustum_culling::context->shadowArgs[camIndex].second;
	shadowmapData = vec[cascadeIndex];
}
VENGINE_UNITY_EXTERN void AddCullVolume(
	TRS const& trs) {
	frustum_culling::context->bboxVolumes.emplace_back(trs);
}
VENGINE_UNITY_EXTERN void ClearCullVolume() {
	frustum_culling::context->bboxVolumes.clear();
}
}// namespace toolhub::renderer
//#define EXPORT_EXE
#ifdef EXPORT_EXE
static thread_local double value = 0;
static std::atomic_size_t i;
int main() {
	tf::Executor e(std::thread::hardware_concurrency());
	tf::Taskflow f;
	f.emplace_all(
		[] {},
		[](size_t i) { value = pow(value + 1e-34, 2.2); },
		std::numeric_limits<size_t>::max(), e.num_workers());
	e.run(std::move(f)).wait();
	std::cout << i << '\n';
	return 0;

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
		BindSunLight(vec3(1, 0, 0), vec3(0, 1, 0), vec3(0, 0, 1), 500);
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
			vec3(0),
			float(60 * Deg2Rad),
			1,
			2048,
			0.3f};
		UpdateCullObject(tt, 0);
		ClearCSM();
		BindCSM(
			csmArg,
			cascades.data(),
			cascades.size());
		for (int i = 0; i < 10240; ++i) {
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