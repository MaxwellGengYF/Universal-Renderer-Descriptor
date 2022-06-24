
#include <Utility/StackAllocator.h>
#include <Utility/ObjectStackAlloc.h>
namespace vstd {
StackAllocator::StackAllocator(
	uint64 initCapacity,
	StackAllocatorVisitor* visitor)
	: capacity(initCapacity),
	  visitor(visitor) {
}
StackAllocator::Chunk StackAllocator::Allocate(uint64 targetSize) {
	for (auto&& i : allocatedBuffers) {
		if (i.leftSize >= targetSize) {
			Buffer* bf = &i;
			auto ofst = bf->fullSize - bf->leftSize;
			bf->leftSize -= targetSize;
			return {
				bf->handle,
				ofst};
		}
	}

	while (capacity < targetSize) {
		capacity = std::max<uint64>(capacity + 1, capacity * 1.5);
	}
	auto newHandle = visitor->Allocate(capacity);
	allocatedBuffers.push_back(Buffer{
		newHandle,
		capacity,
		capacity - targetSize});
	return {
		newHandle,
		0};
	//TODO: return
}
StackAllocator::Chunk StackAllocator::Allocate(
	uint64 targetSize,
	uint64 align) {
	targetSize = std::max(targetSize, align);
	auto CalcAlign = [](uint64 value, uint64 align) -> uint64 {
		return (value + (align - 1)) & ~(align - 1);
	};
	struct Result {
		uint64 offset;
		uint64 leftSize;
	};
	auto GetLeftSize = [&](uint64 leftSize, uint64 size) -> vstd::optional<Result> {
		uint64 offset = size - leftSize;
		uint64 alignedOffset = CalcAlign(offset, align);
		uint64 afterAllocSize = targetSize + alignedOffset;
		if (afterAllocSize > size) return {};
		return Result{alignedOffset, size - afterAllocSize};
	};
	for (auto&& i : allocatedBuffers) {
		auto result = GetLeftSize(i.leftSize, i.fullSize);
		if (!result) continue;
		Buffer* bf = &i;
		uint64 offset = result->offset;
		bf->leftSize = result->leftSize;
		return {
			bf->handle,
			offset};
	}
	while (capacity < targetSize) {
		capacity = std::max<uint64>(capacity + 1, capacity * 1.5);
	}
	auto newHandle = visitor->Allocate(capacity);
	allocatedBuffers.push_back(Buffer{
		newHandle,
		capacity,
		capacity - targetSize});
	return {
		newHandle,
		0};
}
void StackAllocator::Clear() {
	switch (allocatedBuffers.size()) {
		case 0: break;
		case 1: {
			auto&& i = allocatedBuffers[0];
			i.leftSize = i.fullSize;
		} break;
		default: {
			size_t sumSize = 0;
			for (auto&& i : allocatedBuffers) {
				sumSize += i.fullSize;
				visitor->DeAllocate(i.handle);
			}
			allocatedBuffers.clear();
			allocatedBuffers.push_back(Buffer{
				.handle = visitor->Allocate(sumSize),
				.fullSize = sumSize,
				.leftSize = 0});
		} break;
	}
}
StackAllocator::~StackAllocator() {
	for (auto&& i : allocatedBuffers) {
		visitor->DeAllocate(i.handle);
	}
}
void ObjectStackAlloc::Clear() {
	for (auto&& i : ptr) {
		i.disposer(i.ptr);
	}
	ptr.clear();
	alloc.Clear();
}
ObjectStackAlloc::ObjectStackAlloc(
	uint64 initByteSize)
	: alloc(initByteSize, &visitor) {}
ObjectStackAlloc::~ObjectStackAlloc() {
	for (auto&& i : ptr) {
		i.disposer(i.ptr);
	}
}
void* ObjectStackAlloc::AllocateMemory(size_t size, size_t align, vstd::funcPtr_t<void(void*)> disposer) {
	StackAllocator::Chunk chunk = alloc.Allocate(size, align);
	auto result = reinterpret_cast<void**>(chunk.handle + chunk.offset);
	ptr.emplace_back(AllocChunk{
		.ptr = result,
		.disposer = disposer});
	return result;
}
uint64 DefaultMallocVisitor::Allocate(uint64 size) {
	return reinterpret_cast<uint64>(malloc(size));
}
void DefaultMallocVisitor::DeAllocate(uint64 handle) {
	free(reinterpret_cast<void*>(handle));
}
uint64 VEngineMallocVisitor::Allocate(uint64 size) {
	return reinterpret_cast<uint64>(vengine_malloc(size));
}
void VEngineMallocVisitor::DeAllocate(uint64 handle) {
	vengine_free(reinterpret_cast<void*>(handle));
}
}// namespace vstd