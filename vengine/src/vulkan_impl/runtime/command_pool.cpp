#include "command_pool.h"
namespace toolhub::vk {
CommandPool::CommandPool(Device const* device)
	: Resource(device) {
	VkCommandPoolCreateInfo createInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
	createInfo.queueFamilyIndex = device->computeFamily;
	createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	ThrowIfFailed(vkCreateCommandPool(device->device, &createInfo, Device::Allocator(), &pool));
	/*
	VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr, VK_FENCE_CREATE_SIGNALED_BIT};
	vkCreateFence(
		device->device,
		&fenceInfo,
		Device::Allocator(),
		&syncFence);*/
}
CommandPool::~CommandPool() {
	if (pool)
		vkDestroyCommandPool(device->device, pool, Device::Allocator());
}
CommandPool::CommandPool(CommandPool&& v)
	: Resource(v) {
	pool = v.pool;
	v.pool = nullptr;
}
}// namespace toolhub::vk