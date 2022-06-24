#pragma once
#include <vulkan_impl/vulkan_include.h>
#include <vulkan_impl/gpu_allocator/vk_mem_alloc.h>
namespace toolhub::vk {
class GPUAllocator;
struct VmaImage {
	VkImage image;
	VmaAllocation alloc;
	VmaImage(
		GPUAllocator& alloc,
        VkImageType dimension,
		uint3 size,
        uint mip,
        uint arrLayer,
        VkFormat format);
	~VmaImage();

private:
	VmaAllocator allocator;
};
}// namespace toolhub::vk