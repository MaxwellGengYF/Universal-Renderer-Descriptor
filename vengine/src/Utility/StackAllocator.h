#pragma once
#include <Common/Common.h>
#include <Common/TreeMap.h>
namespace vstd {
class StackAllocatorVisitor {
public:
	virtual uint64 Allocate(uint64 size) = 0;
	virtual void DeAllocate(uint64 handle) = 0;
};
class VENGINE_DLL_COMMON DefaultMallocVisitor : public StackAllocatorVisitor {
public:
	uint64 Allocate(uint64 size) override;
	void DeAllocate(uint64 handle) override;
};
class VENGINE_DLL_COMMON VEngineMallocVisitor : public StackAllocatorVisitor {
public:
	uint64 Allocate(uint64 size) override;
	void DeAllocate(uint64 handle) override;
};
class VENGINE_DLL_COMMON StackAllocator {
	StackAllocatorVisitor* visitor;
	uint64 capacity;
	struct Buffer {
		uint64 handle;
		uint64 fullSize;
		uint64 leftSize;
	};
	vstd::vector<Buffer> allocatedBuffers;

public:
	StackAllocator(
		uint64 initCapacity,
		StackAllocatorVisitor* visitor);
	~StackAllocator();
	struct Chunk {
		uint64 handle;
		uint64 offset;
	};
	Chunk Allocate(
		uint64 targetSize);
	Chunk Allocate(
		uint64 targetSize,
		uint64 align);
	void Clear();
};
}// namespace vstd