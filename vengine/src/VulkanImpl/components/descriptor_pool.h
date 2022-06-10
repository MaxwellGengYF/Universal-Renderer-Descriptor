#pragma once
#include <vulkan_initializer.hpp>
#include <components/resource.h>
namespace toolhub::vk {
class DescriptorPool : public Resource {
protected:
	VkDescriptorPool pool;
	vstd::vector<VkDescriptorSet> sets;

public:
	DescriptorPool(Device const* device);
	~DescriptorPool();
	VkDescriptorSet Allocate(
		VkDescriptorSetLayout layout);
	void Clear();
};
}// namespace toolhub::vk