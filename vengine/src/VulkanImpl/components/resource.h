#pragma once
#include <vulkan_include.h>
#include <components/device.h>
namespace toolhub::vk {
class Resource {
protected:
	Device const* device;

public:
	Resource(Device const* device) : device(device) {}
	virtual ~Resource() = default;
};
}// namespace toolhub::vk