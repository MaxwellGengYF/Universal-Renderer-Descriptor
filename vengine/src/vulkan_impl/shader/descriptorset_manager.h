#pragma once
#include <vulkan_include.h>
#include <Common/small_vector.h>
#include <shader/descriptor_pool.h>
#include <types/bind_desriptor.h>
#include <shared_mutex>
#include <Utility/StackAllocator.h>
#include <Common/LockFreeArrayQueue.h>
namespace toolhub::vk {
class DescriptorSetManager;
class CommandBuffer;
class ResStateTracker;
class DescriptorSetManager : public Resource {
	friend class Device;

public:
	struct DescriptorSets {
		vstd::small_vector<VkDescriptorSet> sets;
		vstd::spin_mutex mtx;
		~DescriptorSets();
	};

private:
	size_t glbIndex = 0;
	using DescSetMap = vstd::HashMap<VkDescriptorSetLayout, DescriptorSets>;
	DescSetMap descSets;
	vstd::LockFreeArrayQueue<VkDescriptorSetLayout> removeList;
	vstd::vector<std::pair<DescSetMap::Index, VkDescriptorSet>> allocatedSets;
	vstd::vector<VkWriteDescriptorSet> computeWriteRes;
	vstd::StackAllocator stackAlloc;
	DescriptorSetManager(DescriptorSetManager&&) = delete;
	DescriptorSetManager(DescriptorSetManager const&) = delete;

public:
	static void DestroyPipelineLayout(VkDescriptorSetLayout layout);
	DescriptorSetManager(Device const* device);
	~DescriptorSetManager();
	VkDescriptorSet Allocate(
		ResStateTracker& stateTracker,
		VkDescriptorSetLayout layout,
		vstd::span<VkDescriptorType const> descTypes,
		vstd::span<BindResource const> descriptors);
	void EndFrame();
};
}// namespace toolhub::vk