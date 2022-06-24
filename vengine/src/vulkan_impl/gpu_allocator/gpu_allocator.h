#pragma once
#include <vulkan_impl/components/resource.h>
#include <vulkan_impl/gpu_allocator/vk_mem_alloc.h>
#include <vulkan_impl/gpu_allocator/vma_buffer.h>
#include <vulkan_impl/gpu_allocator/vma_image.h>
namespace toolhub::vk {

class GPUAllocator : public Resource {
	friend struct VmaBuffer;
	friend struct VmaImage;
	VmaAllocator allocator;

public:
	GPUAllocator(Device const* device);
	~GPUAllocator();
};
}// namespace toolhub::vk