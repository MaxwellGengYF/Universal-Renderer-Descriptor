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
	vstd::vector<VkWriteDescriptorSet> bindlessWriteRes;
	vstd::DefaultMallocVisitor mallocVisitor;
	vstd::StackAllocator stackAlloc;
	vstd::StackAllocator bindlessStackAlloc;
	void InitBindless();

public:
	VkDescriptorSetLayout BindlessBufferLayout() const { return bindlessBufferSetLayout; }
	VkDescriptorSet BindlessBufferSet() const { return bindlessBufferSet; }
	VkDescriptorSetLayout BindlessTexLayout() const { return bindlessTexSetLayout; }
	VkDescriptorSet BindlessTexSet() const { return bindlessTexSet; }
	VkDescriptorSetLayout SamplerSetLayout() const { return samplerSetLayout; }
	VkDescriptorSet SamplerSet() const { return samplerSet; }
	DescriptorPool* Pool() { return &pool; }
	DescriptorSetManager(Device const* device);
	~DescriptorSetManager();
	void DestroyPipelineLayout(VkDescriptorSetLayout layout);
	VkDescriptorSet Allocate(
		VkDescriptorSetLayout layout,
		vstd::span<VkDescriptorType const> descTypes,
		vstd::span<BindResource const> descriptors);
	void EndFrame();
	void AddBindlessUpdateCmd(size_t index, BufferView const& buffer);
	void AddBindlessUpdateCmd(size_t index, TexView const& tex);
	void UpdateBindless();
};
}// namespace toolhub::vk