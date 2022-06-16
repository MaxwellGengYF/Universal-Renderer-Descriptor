#include <runtime/descriptorset_manager.h>
namespace toolhub::vk {
DescriptorSetManager::DescriptorSetManager(Device const* device)
	: pool(device), Resource(device) {}
namespace detail {
static thread_local DescriptorPool* gDescriptorPool;
}
DescriptorSetManager::DescriptorSets::~DescriptorSets() {
	if (!sets.empty())
		detail::gDescriptorPool->Destroy(sets);
}

void DescriptorSetManager::DestroyPipelineLayout(VkDescriptorSetLayout layout) {
	detail::gDescriptorPool = &pool;
	std::lock_guard lck(mtx);
	descSets.Remove(layout);
}
VkDescriptorSet DescriptorSetManager::Allocate(
	VkDescriptorSetLayout layout,
	vstd::span<VkDescriptorType> descTypes,
	vstd::span<BindDescriptor> descriptors) {
	assert(descTypes.size() == descriptors.size());
	decltype(descSets)::Index ite;
	{
		std::shared_lock lck(mtx);
		ite = descSets.Find(layout);
		if (!ite) {
			lck.unlock();
			std::lock_guard lck(mtx);
			ite = descSets.Emplace(layout);
		}
	}
	VkDescriptorSet result;
	{
		std::lock_guard lck(ite.Value().mtx);
		auto&& vec = ite.Value().sets;
		if (vec.empty())
			result = pool.Allocate(layout);
		else
			result = vec.erase_last();
	}
	vstd::small_vector<VkWriteDescriptorSet> computeWriteDescriptorSets;
	computeWriteDescriptorSets.push_back_func(descriptors.size(), [&](size_t i) {
		return descriptors[i].visit_or(VkWriteDescriptorSet(), [&](auto&& v) {
			return vks::initializers::writeDescriptorSet(result, descTypes[i], i, &v);
		});
	});
	vkUpdateDescriptorSets(device->device, computeWriteDescriptorSets.size(), computeWriteDescriptorSets.data(), 0, nullptr);
}
DescriptorSetManager::~DescriptorSetManager() {
	detail::gDescriptorPool = &pool;
	descSets.Clear();
}
}// namespace toolhub::vk