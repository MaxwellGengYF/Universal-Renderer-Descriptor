#pragma once
#include <components/resource.h>
#include <runtime/command_buffer.h>
namespace toolhub::vk {
class FrameResource;
class CommandPool : public Resource {
	friend class FrameResource;
	VkCommandPool pool;

public:
	CommandPool(Device const* device);
	CommandPool(CommandPool&& v);
	~CommandPool();
	CommandPool(CommandPool const&) = delete;
};
}// namespace toolhub::vk
