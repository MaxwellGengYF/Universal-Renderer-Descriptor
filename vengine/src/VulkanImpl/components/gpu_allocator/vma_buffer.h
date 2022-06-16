#pragma once
#include <vulkan_include.h>
#include <components/gpu_allocator/vk_mem_alloc.h>
namespace toolhub::vk {
class GPUAllocator;
struct VmaBuffer {
	enum class RWState : vbyte {
		None,
		Readback,
		Upload
	};
	VkBuffer buffer;
	VmaAllocation alloc;
	void* mappedPtr;
	VmaBuffer(
		GPUAllocator& alloc,
		size_t byteSize,
		VkBufferUsageFlagBits usage,
		bool crossQueueShared,
		RWState hostRW);
	~VmaBuffer();

private:
	VmaAllocator allocator;
};
}// namespace toolhub::vk