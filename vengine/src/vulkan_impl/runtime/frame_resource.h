#pragma once
#include <vulkan_impl/components/resource.h>
#include "command_buffer.h"
#include <Utility/StackAllocator.h>
namespace toolhub::vk {
class CommandPool;
class FrameResource : public Resource {
	friend class CommandBuffer;
	vstd::small_vector<VkCommandBuffer> allocatedBuffers;
	vstd::small_vector<VkCommandBuffer> needExecuteBuffers;
	CommandPool* pool;
	VkFence syncFence;
	VkSemaphore semaphore;
	bool executing = false;
	void ReleaseCmdBuffer(VkCommandBuffer buffer);

public:
	FrameResource(Device const* device, CommandPool* pool);
	~FrameResource();
	FrameResource(FrameResource const&) = delete;
	FrameResource(FrameResource&&) = delete;
	vstd::optional<CommandBuffer> AllocateCmdBuffer();
	void Execute(FrameResource const* lastFrame);
	void Wait();
};
}// namespace toolhub::vk