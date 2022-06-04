#pragma once
#include <Renderer/Camera.h>
namespace toolhub {
struct QuadNode {
	static constexpr size_t CHILD_COUNT = 4;
	uint layer;
	vec2 pos;
	float size;
	bool isLeaf = true;
	QuadNode* childNodes[CHILD_COUNT];
	QuadNode(
		uint layer,
		vec2 pos,
		float size);
};
class QuadTree : public vstd::IOperatorNewBase {
	QuadNode* root;
	vstd::Pool<QuadNode, VEngine_AllocType::VEngine, true> nodePool;
	vstd::vector<float> distances;
	vstd::vector<uint> instanceLayer;
	vstd::vector<vec4> cullResult;
	float crossFade;
	vec3 camPos;

	void CombineTree(QuadNode* node);
	void SeparateTree(QuadNode* node);

public:
	vec2 bbMinPos;
	vec2 bbMaxPos;
	QuadTree(vec2 pos, float rootSize);
	~QuadTree();
	void SetLodDistance(std::pair<float, int> const* distance, size_t lodCount, float crossFade);
	void SetCamera(vec3 camPos);
	void UpdateTree();
	void Cull(CamArgs const& cam, float minHeight, float maxHeight);
	std::pair<vec4 const* const, size_t> GetCullResult() {
		return {cullResult.data(), cullResult.size()};
	}
};
}// namespace toolhub