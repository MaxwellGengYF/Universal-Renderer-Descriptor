#include <TextureTools/Allocator.h>
#include <stb/stb_image_write.h>
namespace textools {

Allocator::Allocator() {}
Allocator::~Allocator() {}
void Allocator::Reset(uint resolution) {
	if (pool) pool.Delete();
	if (resolution <= 256) pool.New(64);
	else if (resolution <= 512)
		pool.New(128);
	else
		pool.New(256);
	allocLayers.clear();
	size_t tarSize = 1;
	size_t allocLayerSize = 1;
	while (tarSize < resolution) {
		tarSize *= 2;
		allocLayerSize++;
	}
	allocLayers.resize(allocLayerSize);
	root = pool->New(pool, uint2(0), tarSize, 0, nullptr);
	AddToList(root);
}
QuadNode::QuadNode(QuadNodePool* pool, uint2 offset, uint size, uint layer, QuadNode* parent) {
	this->pool = pool;
	memset(childs, 0, QuadNode::CHILD_BYTE_SIZE);
	this->offset = offset;
	this->size = size;
	this->layer = layer;
	this->parent = parent;
}
void Allocator::AddToList(QuadNode* node) {
	auto&& vec = allocLayers[node->layer];
	node->vecIndex = vec.size();
	vec.emplace_back(node);
}
void Allocator::RemoveFromList(QuadNode* node) {
	auto&& vec = allocLayers[node->layer];
	if (node->vecIndex != vec.size() - 1) {
		auto last = vec.erase_last();
		vec[node->vecIndex] = last;
		last->vecIndex = node->vecIndex;
	} else {
		vec.erase_last();
	}
	node->vecIndex = std::numeric_limits<size_t>::max();
}
QuadNode* Allocator::Allocate(uint tarResolution) {
	if (root == nullptr || tarResolution == 0 || tarResolution > root->size) return nullptr;
	size_t tarLayer = 0;
	size_t size = root->size;
	while (true) {
		size_t halfSize = size / 2;
		if (halfSize >= tarResolution) {
			tarLayer++;
			size = halfSize;
		} else {
			break;
		}
	}
	return AllocLayer(tarLayer);
}
void Allocator::DeAllocate(QuadNode* node) {
	AddToList(node);
	if (node->parent)
		Combine(node->parent);
}
void Allocator::Combine(QuadNode* node) {
	for (auto&& i : node->childs) {
		if (!i->IsAllocatable()) return;
	}
	for (auto&& i : node->childs) {
		if (i) {
			RemoveFromList(i);
			pool->Delete(i);
		}
	}
	memset(node->childs, 0, QuadNode::CHILD_BYTE_SIZE);
	AddToList(node);
	if (node->parent) Combine(node->parent);
}
QuadNode* Allocator::AllocLayer(size_t tarLayer) {
	if (!allocLayers[tarLayer].empty()) {
		auto r = allocLayers[tarLayer].erase_last();
		r->vecIndex = std::numeric_limits<size_t>::max();
		return r;
	}
	if (tarLayer == 0) return nullptr;
	size_t startAllocLayer = tarLayer;
	for (auto i : vstd::range(tarLayer - 1, -1, -1)) {
		if (allocLayers[i].empty()) continue;
		for (auto j : vstd::range(i, tarLayer)) {
			auto&& vec = allocLayers[j];
			auto last = vec.erase_last();
			last->vecIndex = std::numeric_limits<size_t>::max();
			static constexpr uint X_OFFSET[4] = {0, 1, 0, 1};
			static constexpr uint Y_OFFSET[4] = {0, 0, 1, 1};
			uint halfSize = last->size / 2;
			for (auto v : vstd::range(QuadNode::CHILD_SIZE)) {
				last->childs[v] = pool->New(pool, last->offset + uint2(X_OFFSET[v], Y_OFFSET[v]) * halfSize, halfSize, j + 1, last);
				AddToList(last->childs[v]);
			}
		}
		break;
	}
	if (allocLayers[tarLayer].empty()) return nullptr;
	auto r = allocLayers[tarLayer].erase_last();
	r->vecIndex = std::numeric_limits<size_t>::max();
	return r;
}
QuadNode::~QuadNode() {
	for (auto&& i : childs) {
		if (i) pool->Delete(i);
	}
}
#ifdef _DEBUG
void Allocator::PrintVecSize() {
	for (auto&& i : allocLayers) {
		std::cout << i.size() << ' ';
	}
	std::cout << '\n';
}
#endif
////////////C API
VENGINE_UNITY_EXTERN Allocator* TexTools_CreateAlloc() {
	return new Allocator();
}
VENGINE_UNITY_EXTERN void TexTools_DestroyAlloc(Allocator* ptr) {
	delete ptr;
}
struct TexResult {
	uint2 offset;
	uint size;
};
VENGINE_UNITY_EXTERN void TexTools_CreateAtlas(
	Allocator* alloc,
	uint atlasRes,
	std::pair<uint const*, size_t> sizes,
	TexResult* results) {
	alloc->Reset(atlasRes);
	for (auto i : vstd::range(sizes.second)) {
		auto chunk = alloc->Allocate(sizes.first[i]);
		if (!chunk) {
			results[i] = TexResult{
				.offset = uint2(0),
				.size = 0};
			continue;
		}
		results[i] = TexResult{
			.offset = chunk->offset,
			.size = chunk->size};
	}
}
}// namespace textools
