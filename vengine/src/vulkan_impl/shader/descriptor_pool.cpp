#include <shader/descriptor_pool.h>
#include <Common/small_vector.h>
namespace toolhub::vk {

//TODO:
DescriptorPool::DescriptorPool(Device const* device)
	: Resource(device) {
	VkDescriptorPoolCreateInfo createInfo{
		VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		nullptr,
		VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT | VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT};
	createInfo.maxSets = MAX_SET;
	vstd::small_vector<VkDescriptorPoolSize> descSizes;
	descSizes.push_back(VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, MAX_RES});
	descSizes.push_back(VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, MAX_WRITE_TEX});
	descSizes.push_back(VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, MAX_RES * 2});
	descSizes.push_back(VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLER, MAX_SAMP});
	createInfo.pPoolSizes = descSizes.data();
	createInfo.poolSizeCount = descSizes.size();

	ThrowIfFailed(vkCreateDescriptorPool(device->device, &createInfo, Device::Allocator(), &pool));
	//TODO
}
DescriptorPool::~DescriptorPool() {
	vkDestroyDescriptorPool(device->device, pool, Device::Allocator());
}
VkDescriptorSet DescriptorPool::Allocate(
	VkDescriptorSetLayout layout,
	VkDescriptorSetVariableDescriptorCountAllocateInfo* info) {
	VkDescriptorSet descriptorSet;
	VkDescriptorSetAllocateInfo allocInfo =
		vks::initializers::descriptorSetAllocateInfo(pool, &layout, 1);
	allocInfo.pNext = info;
	ThrowIfFailed(vkAllocateDescriptorSets(device->device, &allocInfo, &descriptorSet));
	return descriptorSet;
}
void DescriptorPool::Destroy(VkDescriptorSet set) {
	ThrowIfFailed(vkFreeDescriptorSets(device->device, pool, 1, &set));
}
void DescriptorPool::Destroy(vstd::span<VkDescriptorSet> set) {
	ThrowIfFailed(vkFreeDescriptorSets(device->device, pool, set.size(), set.data()));
}
}// namespace toolhub::vk