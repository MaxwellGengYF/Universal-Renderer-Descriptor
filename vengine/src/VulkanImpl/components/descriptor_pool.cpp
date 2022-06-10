#include <components/descriptor_pool.h>
namespace toolhub::vk {
//TODO:
DescriptorPool::DescriptorPool(Device const* device)
	: Resource(device) {
	//TODO
}
DescriptorPool::~DescriptorPool() {
	vkDestroyDescriptorPool(device->device, pool, &device->allocator);
}
VkDescriptorSet DescriptorPool::Allocate(
	VkDescriptorSetLayout layout) {
	VkDescriptorSet descriptorSet;
	VkDescriptorSetAllocateInfo allocInfo =
		vks::initializers::descriptorSetAllocateInfo(pool, &layout, 1);
	ThrowIfFailed(vkAllocateDescriptorSets(device->device, &allocInfo, &descriptorSet));
	sets.emplace_back(descriptorSet);
	return descriptorSet;
}
void DescriptorPool::Clear() {
	vkFreeDescriptorSets(device->device, pool, sets.size(), sets.data());
	sets.clear();
}
}// namespace toolhub::vk