#pragma once
#include <vulkan_include.h>
#include <Common/small_vector.h>
#include <components/descriptor_pool.h>
#include <types/bind_desriptor.h>
#include <shared_mutex>
namespace toolhub::vk {
class DescriptorSetManager;
class DescriptorSetManager : public Resource {
public:
	struct DescriptorSets {
		vstd::small_vector<VkDescriptorSet> sets;
		vstd::spin_mutex mtx;
		~DescriptorSets();
	};

private:
	std::shared_mutex mtx;
	vstd::HashMap<VkDescriptorSetLayout, DescriptorSets> descSets;
	DescriptorPool pool;

public:
	DescriptorSetManager(Device const* device);
	~DescriptorSetManager();
	void DestroyPipelineLayout(VkDescriptorSetLayout layout);
	VkDescriptorSet Allocate(
		VkDescriptorSetLayout layout,
        vstd::span<VkDescriptorType> descTypes,
		vstd::span<BindDescriptor> descriptors);
};
}// namespace toolhub::vk