#pragma once
#include <Common/Common.h>
#include <glm/Include.h>
using namespace glm;
namespace textools {
/*
layer 0 - N
0 = max resolution
N = 1x1
*/
struct QuadNode;
using QuadNodePool = vstd::Pool<QuadNode, VEngine_AllocType::VEngine, true>;
struct QuadNode {
	static constexpr size_t CHILD_SIZE = 4;
	static constexpr size_t CHILD_BYTE_SIZE = CHILD_SIZE * sizeof(void*);
	QuadNodePool* pool;
	QuadNode* childs[CHILD_SIZE];
	QuadNode* parent;
	uint2 offset;
	uint size;
	uint layer;
	size_t vecIndex = std::numeric_limits<size_t>::max();
	bool IsAllocatable() const { return vecIndex != std::numeric_limits<size_t>::max(); }
	QuadNode(QuadNodePool* pool,
			 uint2 offset,
			 uint res,
			 uint layer,
			 QuadNode* parent);
	~QuadNode();
};

class Allocator : public vstd::IOperatorNewBase {
	QuadNode* root = nullptr;
	vstd::optional<QuadNodePool> pool;
	vstd::vector<vstd::vector<QuadNode*>> allocLayers;
	void AddToList(QuadNode* node);
	void RemoveFromList(QuadNode* node);
	void Combine(QuadNode* node);
	QuadNode* AllocLayer(size_t tarLayer);

public:
	uint Resolution() const { return root->size; }
	Allocator();
	~Allocator();
	void Reset(uint resolution);
	QuadNode* Allocate(uint tarResolution);
	void DeAllocate(QuadNode* node);
#ifdef _DEBUG
	void PrintVecSize();
#endif
};
}// namespace textools