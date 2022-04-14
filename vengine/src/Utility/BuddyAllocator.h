#pragma once
#include <Common/Common.h>
#include <Common/functional.h>
using uint = uint32_t;
class BuddyLinkedList;
class BuddyBinaryTree;
class BuddyAllocator;
struct BuddyNode
{
	friend class BuddyLinkedList;
	friend class BuddyBinaryTree;
	friend class BuddyAllocator;
private:
	BuddyNode* left;
	BuddyNode* right;
	BuddyNode* parent;
	uint64_t size;
	uint64_t allocatedMem;
	uint layer;
	bool isUsing;
	void* resourceBlock;

	uint64 arrayIndex;

public:
	uint64_t GetSize() const { return size; }
	uint64_t GetPosition() const { return allocatedMem; }
	uint GetLayer() const { return layer; }
	template<typename T>
	T* GetBlockResource() const
	{
		return (T*)resourceBlock;
	}
};

class BuddyLinkedList
{

private:
	vstd::vector<BuddyNode*> nodes;

public:
	BuddyLinkedList() {}
	uint64_t size() const noexcept { return nodes.size(); }
	void Add(BuddyNode* node)noexcept;
	BuddyNode* RemoveHead() noexcept;
	void Remove(BuddyNode* node)noexcept;
};
class BuddyBinaryTree
{

private:
	BuddyNode* rootNode = nullptr;
public:
	BuddyBinaryTree(uint64_t initSize, void* ptr, BuddyLinkedList* links, vstd::Pool<BuddyNode>* nodePool)noexcept;
	static void Split(uint layer, BuddyLinkedList* links, vstd::Pool<BuddyNode>* nodePool)noexcept;
	static void Combine(BuddyNode* parentNode, BuddyLinkedList* links, vstd::Pool<BuddyNode>* nodePool)noexcept;
	static void TryCombine(BuddyNode* currentNode, BuddyLinkedList* links, vstd::Pool<BuddyNode>* nodePool)noexcept;
};

class VENGINE_DLL_COMMON BuddyAllocator : public vstd::IOperatorNewBase
{
	vstd::Pool<BuddyNode> nodePool;
	vstd::Pool<BuddyBinaryTree> treePool;
	vstd::function<void* (uint64_t)> resourceBlockConstruct;
	vstd::function<void(void*)> resourceBlockDestructor;
	vstd::vector<BuddyLinkedList> linkList;
	uint64_t initSize;
	uint binaryLayer;
	uint treeCapacity;
	vstd::vector<std::pair<BuddyBinaryTree*, void*>> allocatedTree;
	inline static constexpr uint GetPow2(uint i)
	{
		uint sm = 1;
		for (uint a = 0; a < i; ++a)
			sm *= 2;
		return sm;
	}
	BuddyNode* Allocate_T(uint targetLayer);
public:
	BuddyAllocator(uint binaryLayer, uint treeCapacity, uint64_t initSize, vstd::function<void* (uint64_t)> && blockConstructor, vstd::function<void(void*)> && resourceBlockDestructor);
	void Free(BuddyNode* node);
	BuddyNode* Allocate(uint targetLayer);
	~BuddyAllocator();
};