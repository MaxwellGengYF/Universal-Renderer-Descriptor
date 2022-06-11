#include <components/descriptor_pool.h>
namespace toolhub::vk {
//TODO:
DescriptorPool::DescriptorPool(Device const* device)
	: Resource(device) {
	VkDescriptorPoolCreateInfo createInfo{
		VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		nullptr,
		VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT | VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT
	};
	vkCreateDescriptorPool(device->device, &createInfo, Device::Allocator(), &pool);
	//TODO
}
DescriptorPool::~DescriptorPool() {
	vkDestroyDescriptorPool(device->device, pool, Device::Allocator());
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