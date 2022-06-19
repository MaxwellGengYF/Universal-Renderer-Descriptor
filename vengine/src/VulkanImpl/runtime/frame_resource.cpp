#include <runtime/frame_resource.h>
#include <vulkan_initializer.hpp>
#include <runtime/command_pool.h>
namespace toolhub::vk {
FrameResource::FrameResource(Device const* device, CommandPool* pool)
	: Resource(device), pool(pool) {
	VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	ThrowIfFailed(vkCreateFence(device->device, &fenceInfo, Device::Allocator(), &syncFence));
	vkResetFences(device->device, 1, &syncFence);
	VkSemaphoreCreateInfo semaphoreInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
	vkCreateSemaphore(
		device->device,
		&semaphoreInfo,
		Device::Allocator(),
		&semaphore);
}
FrameResource::~FrameResource() {
	vkDestroySemaphore(device->device, semaphore, Device::Allocator());
	vkDestroyFence(device->device, syncFence, Device::Allocator());
}
void FrameResource::ReleaseCmdBuffer(VkCommandBuffer buffer) {
	allocatedBuffers.emplace_back(buffer);
}
vstd::optional<CommandBuffer> FrameResource::AllocateCmdBuffer() {
	if (executing) return {};
	VkCommandBuffer vkCmdBuffer;
	if (allocatedBuffers.empty()) {
		VkCommandBufferAllocateInfo allocInfo = vks::initializers::commandBufferAllocateInfo(
			pool->pool,
			VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			1);
		ThrowIfFailed(vkAllocateCommandBuffers(device->device, &allocInfo, &vkCmdBuffer));
	} else {
		vkCmdBuffer = allocatedBuffers.erase_last();
	}
	needExecuteBuffers.emplace_back(vkCmdBuffer);
	return CommandBuffer(device, vkCmdBuffer, this);
}
void FrameResource::Execute(FrameResource const* lastFrame) {
	if (executing && !needExecuteBuffers.empty()) return;
	VkSubmitInfo computeSubmitInfo = vks::initializers::submitInfo();
	computeSubmitInfo.commandBufferCount = needExecuteBuffers.size();
	computeSubmitInfo.pCommandBuffers = needExecuteBuffers.data();
	if (lastFrame) {
		computeSubmitInfo.waitSemaphoreCount = 1;
		VkSemaphore phore = lastFrame->semaphore;
		computeSubmitInfo.pWaitSemaphores = &phore;
	}
	/*
	computeSubmitInfo.waitSemaphoreCount = 1;
	computeSubmitInfo.pWaitSemaphores = &graphics.semaphore;
	computeSubmitInfo.pWaitDstStageMask = &waitStageMask;
	computeSubmitInfo.signalSemaphoreCount = 1;
	computeSubmitInfo.pSignalSemaphores = &compute.semaphore;*/
	ThrowIfFailed(vkQueueSubmit(device->computeQueue, 1, &computeSubmitInfo, syncFence));
	executing = true;
	needExecuteBuffers.clear();
}
void FrameResource::Wait() {
	ThrowIfFailed(vkWaitForFences(
		device->device,
		1,
		&syncFence,
		true,
		std::numeric_limits<uint64>::max()));
	executing = false;
	vkResetFences(device->device, 1, &syncFence);
}
}// namespace toolhub::vk