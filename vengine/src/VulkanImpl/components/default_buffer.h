#pragma once
#include <components/resource.h>
#include <components/gpu_allocator/vma_buffer.h>
namespace toolhub::vk {
class GPUAllocator;
class DefaultBuffer : public Resource {
	VmaBuffer vmaBuffer;
	size_t byteSize;

public:
	size_t ByteSize() const { return byteSize; }
	DefaultBuffer(
		Device const* device,
		size_t byteSize,
		VkBufferUsageFlagBits flag,
		bool crossQueue);
	~DefaultBuffer();
};
}// namespace toolhub::vk