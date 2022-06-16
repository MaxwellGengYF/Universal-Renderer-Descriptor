#pragma once
#include <components/resource.h>
#include <components/gpu_allocator/vk_mem_alloc.h>
#include <components/gpu_allocator/vma_buffer.h>
namespace toolhub::vk {

class GPUAllocator : public Resource {
	friend class VmaBuffer;
	VmaAllocator allocator;

public:
	GPUAllocator(Device const* device);
	~GPUAllocator();
};
}// namespace toolhub::vk