#pragma once
#include <vulkan_impl/vulkan_initializer.hpp>
#include <vulkan_impl/components/resource.h>
namespace toolhub::vk {
class DescriptorPool : public Resource {
protected:
	VkDescriptorPool pool;

public:
	static constexpr uint MAX_SET = 81920;
	static constexpr uint MAX_RES = 81920;
	static constexpr uint MAX_SAMP = 16;
	static constexpr uint MAX_WRITE_TEX = 16;
	static constexpr uint MAX_BINDLESS_SIZE = std::numeric_limits<uint16_t>::max();
	DescriptorPool(Device const* device);
	~DescriptorPool();
	VkDescriptorSet Allocate(
		VkDescriptorSetLayout layout,
		VkDescriptorSetVariableDescriptorCountAllocateInfo* info = nullptr);
	void Destroy(VkDescriptorSet set);
	void Destroy(vstd::span<VkDescriptorSet> set);
};
}// namespace toolhub::vk