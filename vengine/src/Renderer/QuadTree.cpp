#include <Renderer/QuadTree.h>
#include <taskflow/taskflow.hpp>
#include <Utility/MathLib.h>
namespace toolhub {
QuadNode::QuadNode(
	uint layer,
	vec2 pos,
	float size)
	: layer(layer),
	  pos(pos),
	  size(size) {
	memset(childNodes, 0, sizeof(size_t) * QuadNode::CHILD_COUNT);
}
void QuadTree::CombineTree(QuadNode* node) {
	if (node->isLeaf) return;
	for (auto&& i : node->childNodes) {
		CombineTree(i);
		nodePool.Delete(i);
	}
	memset(node->childNodes, 0, sizeof(size_t) * QuadNode::CHILD_COUNT);
}
void QuadTree::SeparateTree(QuadNode* node) {
	if (!node->isLeaf) return;
	node->isLeaf = false;
	static const vec2 offsets[4] = {
		vec2(-0.5f, -0.5f),
		vec2(0.5f, -0.5f),
		vec2(-0.5f, 0.5f),
		vec2(0.5f, 0.5f)};
	for (auto i : vstd::range(QuadNode::CHILD_COUNT)) {
		float halfSize = node->size * 0.5f;
		auto child = nodePool.New(node->layer + 1, node->pos + offsets[i] * halfSize, halfSize);
		node->childNodes[i] = child;
	}
}

QuadTree::QuadTree(vec2 pos, float rootSize)
	: nodePool(256) {
	root = nodePool.New(0, pos, rootSize);
}
QuadTree::~QuadTree() {}
void QuadTree::SetLodDistance(float const* distance, size_t lodCount, float crossFade) {
	distances.clear();
	distances.push_back_all(distance, lodCount);
	this->crossFade = crossFade;
}
void QuadTree::SetCamera(vec3 camPos) {
	this->camPos = camPos;
}
void QuadTree::UpdateTree() {
	if (distances.empty()) return;
	enum class Op : vbyte {
		Combine,
		None,
		Separate,
		Stop
	};
	auto CompareDistance = [&](uint layer, float dist) -> Op {
		if (layer >= distances.size() - 1) return Op::Stop;
		if (dist > distances[layer] + crossFade)
			return Op::Combine;
		else if (dist < distances[layer] - crossFade)
			return Op::Separate;
		return Op::None;
	};
	auto Dist = [&](QuadNode* node) -> float {
		vec2 toCam = abs(vec2(camPos.x, camPos.z) - node->pos);
		float halfSize = node->size * 0.5f;
		bool xLarger = toCam.x > halfSize;
		bool yLarger = toCam.y > halfSize;
		if (xLarger && yLarger) {
			return distance(toCam, vec2(halfSize));
		} else if (xLarger) {
			return toCam.x - halfSize;
		} else if (yLarger) {
			return toCam.y - halfSize;
		}
		return 0;
	};
	auto ProcessTree = [&](auto&& ProcessTree, QuadNode* node) -> void {
		switch (CompareDistance(node->layer, Dist(node))) {
			case Op::Combine: {
				CombineTree(node);
			} break;
			case Op::Separate: {
				SeparateTree(node);
				for (auto&& i : node->childNodes) {
					ProcessTree(ProcessTree, i);
				}
			} break;
			case Op::Stop:
				return;
		}
	};
	ProcessTree(ProcessTree, root);
}
void QuadTree::Cull(CamArgs const& cam, float minHeight, float maxHeight) {
	vec4 frustum[6];
	MathLib::GetPerspFrustumPlanes(
		cam.cameraRight,
		cam.cameraUp,
		cam.cameraForward,
		cam.cameraPosition,
		cam.fov, cam.aspect,
		cam.nearPlane,
		cam.farPlane,
		frustum);
	auto minMax = MathLib::GetFrustumBoundingBox(cam.cameraRight, cam.cameraUp, cam.cameraForward, cam.cameraPosition, cam.fov, cam.aspect, cam.nearPlane, cam.farPlane);
	cullResult.clear();
	float heightPos = mix(minHeight, maxHeight, 0.5f);
	float heightExtent = (maxHeight - minHeight) * 0.5f;
	auto CullNode = [&](auto&& CullNode, QuadNode* node) -> void {
		if (!node->isLeaf) {
			for (auto&& i : node->childNodes)
				CullNode(CullNode, i);
			return;
		}
		float halfSize = node->size * 0.5f;
		if (MathLib::BoxIntersect(
				vec3(node->pos.x, heightPos, node->pos.y),
				vec3(halfSize, heightExtent, halfSize),
				frustum,
				minMax.first,
				minMax.second)) {
			cullResult.emplace_back(node);
		}
	};
}
class QuadTreeTask : public vstd::IOperatorNewBase {
	tf::Executor executor;
	tf::Future<void> task;
	bool completed = true;

public:
	using Arg = vstd::span<std::pair<QuadTree*, CamArgs> const>;
	QuadTreeTask(uint threadCount) : executor(threadCount) {}
	void Execute(
		float minHeight,
		float maxHeight,
		Arg const& span) {
		tf::Taskflow flow;
		flow.emplace_all(
			[&](size_t i) {
				auto a = span[i].first;
				a->UpdateTree();
				a->Cull(span[i].second, minHeight, maxHeight);
			},
			span.size(), executor.num_workers());
		task = executor.run(std::move(flow));
		completed = false;
	}
	void Complete() {
		if (!completed) {
			task.wait();
			completed = true;
		}
	}
};
VENGINE_UNITY_EXTERN QuadTree* CreateQuadTree(vec2 pos, float rootSize) {
	return new QuadTree(pos, rootSize);
}
VENGINE_UNITY_EXTERN void DestroyQuadTree(QuadTree* tree) {
	delete tree;
}
VENGINE_UNITY_EXTERN void SetQuadTreeLODDistances(QuadTree* tree, float* distances, uint distCount, float crossFade) {
	tree->SetLodDistance(distances, distCount, crossFade);
}
VENGINE_UNITY_EXTERN void SetQuadTreeCamPos(QuadTree* tree, vec3 camPos) {
	tree->SetCamera(camPos);
}
VENGINE_UNITY_EXTERN QuadTreeTask* CreateTaskflowExecutor(uint threadCount) {
	return new QuadTreeTask(threadCount);
}
VENGINE_UNITY_EXTERN void DestroyTaskflowExecutor(QuadTreeTask* executor) {
	delete executor;
}
VENGINE_UNITY_EXTERN void ExecuteQuadTreeTask(
	QuadTreeTask* executor,
	float minHeight,
	float maxHeight,
	std::pair<QuadTree*, CamArgs>* trees,
	uint treeCount) {
	executor->Execute(minHeight, maxHeight, {trees, treeCount});
}
VENGINE_UNITY_EXTERN void CompleteQuadTreeTask(QuadTreeTask* executor) {
	executor->Complete();
}
VENGINE_UNITY_EXTERN std::pair<QuadNode const* const* const, size_t> GetQuadTreeCullResult(QuadTree* tree) {
	return tree->GetCullResult();
}
}// namespace toolhub
int main() {
	using namespace toolhub;
	QuadTree quadTree(vec2(0), 64);
	QuadTreeTask treeTask(4);
	QuadTreeTask::Arg arg;
	std::pair<QuadTree*, CamArgs> cam;
	std::initializer_list<float> dists = {5, 10, 20, 40};
	quadTree.SetLodDistance(dists.begin(), dists.size(), 0.5);
	quadTree.SetCamera(vec3(0));
	cam.first = &quadTree;
	cam.second.cameraRight = vec3(1, 0, 0);
	cam.second.cameraUp = vec3(0, 1, 0);
	cam.second.cameraForward = vec3(0, 0, 1);
	cam.second.cameraPosition = vec3(0);
	cam.second.aspect = 1;
	cam.second.fov = 0.2f;
	cam.second.nearPlane = 0.3f;
	cam.second.farPlane = 1000.0f;

	treeTask.Execute(0, 1, {&cam, 1});
	treeTask.Complete();
	return 0;
}