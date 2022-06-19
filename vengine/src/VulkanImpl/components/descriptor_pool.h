#pragma once
#include <vulkan_initializer.hpp>
#include <components/resource.h>
namespace toolhub::vk {
class DescriptorPool : public Resource {
protected:
	VkDescriptorPool pool;

public:
	static constexpr uint MAX_SET = 65536;
	static constexpr uint MAX_RES = 65536;
	static constexpr uint MAX_SAMP = 16;
	DescriptorPool(Device const* device);
	~DescriptorPool();
	VkDescriptorSet Allocate(
		VkDescriptorSetLayout layout);
	void Destroy(VkDescriptorSet set);
	void Destroy(vstd::span<VkDescriptorSet> set);
};
}// namespace toolhub::vk