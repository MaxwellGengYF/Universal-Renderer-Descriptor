#pragma once
#include <vulkan_initializer.hpp>
#include <components/resource.h>
namespace toolhub::vk {
class DescriptorPool : public Resource {
protected:
	VkDescriptorPool pool;

public:
	DescriptorPool(Device const* device);
	~DescriptorPool();
	VkDescriptorSet Allocate(
		VkDescriptorSetLayout layout);
	void Destroy(VkDescriptorSet set);
	void Destroy(vstd::span<VkDescriptorSet> set);
};
}// namespace toolhub::vk