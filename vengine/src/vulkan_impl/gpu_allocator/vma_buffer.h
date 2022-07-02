#pragma once
#include <vulkan_include.h>
#include <gpu_allocator/vk_mem_alloc.h>
namespace toolhub::vk {
class GPUAllocator;
enum class RWState : vbyte {
	None,
	Readback,
	Upload,
	Accel
};
struct VmaBuffer {
	VkBuffer buffer;
	VmaAllocation alloc;
	void* mappedPtr;
	VmaBuffer(
		GPUAllocator& alloc,
		size_t byteSize,
		VkBufferUsageFlagBits usage,
		bool crossQueueShared,
		RWState hostRW,
		size_t alignment);
	~VmaBuffer();
	VmaBuffer(VmaBuffer&&) = delete;
	VmaBuffer(VmaBuffer const&) = delete;

private:
	VmaAllocator allocator;
};
}// namespace toolhub::vk