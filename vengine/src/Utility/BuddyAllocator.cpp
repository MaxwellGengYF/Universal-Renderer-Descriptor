
#include <Utility/BuddyAllocator.h>
void BuddyLinkedList::Add(BuddyNode* node) noexcept {
	node->arrayIndex = nodes.size();
	nodes.push_back(node);
}
BuddyNode* BuddyLinkedList::RemoveHead() noexcept {
	return nodes.erase_last();
}
void BuddyLinkedList::Remove(BuddyNode* node) noexcept {
	if (node->arrayIndex != (nodes.size() - 1)) {
		auto last = nodes.erase_last();
		last->arrayIndex = node->arrayIndex;
		nodes[node->arrayIndex] = last;
	} else {
		nodes.erase_last();
	}
}
BuddyBinaryTree::BuddyBinaryTree(uint64_t initSize, void* ptr, BuddyLinkedList* links, vstd::Pool<BuddyNode>* nodePool) noexcept {
	rootNode = nodePool->New();
	memset(rootNode, 0, sizeof(BuddyNode));
	rootNode->size = initSize;
	rootNode->resourceBlock = ptr;
	links[0].Add(rootNode);
}
void BuddyBinaryTree::Split(uint layer, BuddyLinkedList* links, vstd::Pool<BuddyNode>* nodePool) noexcept {
	auto targetNode = links[layer].RemoveHead();
	targetNode->isUsing = true;
	BuddyNode* leftNode = nodePool->New();
	leftNode->size = targetNode->size / 2;
	leftNode->layer = layer + 1;
	leftNode->allocatedMem = targetNode->allocatedMem;
	leftNode->parent = targetNode;
	leftNode->isUsing = false;
	leftNode->resourceBlock = targetNode->resourceBlock;
	BuddyNode* rightNode = nodePool->New();
	memcpy(rightNode, leftNode, sizeof(BuddyNode));
	rightNode->allocatedMem += rightNode->size;
	auto&& lk = links[layer + 1];
	lk.Add(leftNode);
	lk.Add(rightNode);
	targetNode->left = leftNode;
	targetNode->right = rightNode;
}
void BuddyBinaryTree::Combine(BuddyNode* parentNode, BuddyLinkedList* links, vstd::Pool<BuddyNode>* nodePool) noexcept {
	auto&& sonLink = links[parentNode->left->layer];
	sonLink.Remove(parentNode->left);
	sonLink.Remove(parentNode->right);
	nodePool->Delete(parentNode->left);
	nodePool->Delete(parentNode->right);
	parentNode->isUsing = false;
	links[parentNode->layer].Add(parentNode);
}
void BuddyBinaryTree::TryCombine(BuddyNode* currentNode, BuddyLinkedList* links, vstd::Pool<BuddyNode>* nodePool) noexcept {
	while (currentNode->parent != nullptr) {
		currentNode = currentNode->parent;
		if (currentNode->left->isUsing || currentNode->right->isUsing) return;
		Combine(currentNode, links, nodePool);
	}
}
BuddyNode* BuddyAllocator::Allocate_T(uint targetLayer) {
	auto&& curLink = linkList[targetLayer];
	if (curLink.size()) {
		return curLink.RemoveHead();
	} else {
		void* ptr;
		//Split
		uint i = targetLayer;
		[&](){
			while (i > 0) {
				i--;
				if (linkList[i].size() > 0)
					return;
			}
			auto v = targetLayer;
			ptr = resourceBlockConstruct(initSize);
			allocatedTree.push_back({treePool.New(initSize, ptr, linkList.data(), &nodePool), ptr});
			//Add New Binary
		}();
		for (; i < targetLayer; ++i) {
			BuddyBinaryTree::Split(i, linkList.data(), &nodePool);
		}
		return curLink.RemoveHead();
	}
}
BuddyAllocator::BuddyAllocator(uint binaryLayer, uint treeCapacity, uint64_t initSize, vstd::function<void*(uint64_t)>&& blockConstructor, vstd::function<void(void*)>&& resourceBlockDestructor)
	: nodePool((GetPow2(binaryLayer) - 1) * treeCapacity),
	  initSize(initSize),
	  treePool(treeCapacity),
	  linkList(binaryLayer),
	  binaryLayer(binaryLayer),
	  treeCapacity(treeCapacity),
	  resourceBlockConstruct(std::move(blockConstructor)),
	  resourceBlockDestructor(std::move(resourceBlockDestructor)) {
}
void BuddyAllocator::Free(BuddyNode* node) {
	node->isUsing = false;
	linkList[node->layer].Add(node);
	BuddyBinaryTree::TryCombine(node, linkList.data(), &nodePool);
}
BuddyNode* BuddyAllocator::Allocate(uint targetLayer) {
	auto n = Allocate_T(targetLayer);
	n->isUsing = true;
	return n;
}
BuddyAllocator::~BuddyAllocator() {
	for (uint i = 0; i < allocatedTree.size(); ++i) {
		resourceBlockDestructor(allocatedTree[i].second);
	}
}
