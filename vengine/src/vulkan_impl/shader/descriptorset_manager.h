#pragma once
#include <vulkan_impl/vulkan_include.h>
#include <Common/small_vector.h>
#include <vulkan_impl/shader/descriptor_pool.h>
#include <vulkan_impl/types/bind_desriptor.h>
#include <shared_mutex>
#include <Utility/StackAllocator.h>
namespace toolhub::vk {
class DescriptorSetManager;
class CommandBuffer;
class DescriptorSetManager : public Resource {
public:
	struct DescriptorSets {
		vstd::small_vector<VkDescriptorSet> sets;
		vstd::spin_mutex mtx;
		~DescriptorSets();
	};

private:
	std::shared_mutex mtx;
	using DescSetMap = vstd::HashMap<VkDescriptorSetLayout, DescriptorSets>;
	DescSetMap descSets;
	vstd::vector<std::pair<DescSetMap::Index, VkDescriptorSet>> allocatedSets;
	DescriptorPool pool;
	VkDescriptorSetLayout samplerSetLayout;
	VkDescriptorSet samplerSet;
	VkDescriptorSetLayout bindlessTexSetLayout;
	VkDescriptorSet bindlessTexSet;
	VkDescriptorSetLayout bindlessBufferSetLayout;
	VkDescriptorSet bindlessBufferSet;
	std::array<VkSampler, 16> samplers;
	vstd::vector<VkWriteDescriptorSet> computeWriteRes;
	vstd::DefaultMallocVisitor mallocVisitor;
	vstd::StackAllocator stackAlloc;
	void InitBindless();

public:
	VkDescriptorSet SamplerSet() const { return samplerSet; }
	VkDescriptorSetLayout SamplerSetLayout() const { return samplerSetLayout; }
	DescriptorPool* Pool() { return &pool; }
	DescriptorSetManager(Device const* device);
	~DescriptorSetManager();
	void DestroyPipelineLayout(VkDescriptorSetLayout layout);
	VkDescriptorSet Allocate(
		VkDescriptorSetLayout layout,
		vstd::span<VkDescriptorType const> descTypes,
		vstd::span<BindResource const> descriptors);
	void EndFrame();
};
}// namespace toolhub::vk