#pragma once
#include <vulkan_impl/vulkan_include.h>
#include <vulkan_impl/components/device.h>
#include <Common/small_vector.h>
namespace toolhub::vk {

class Resource : public vstd::IOperatorNewBase {
protected:
	Device const* device;

public:
	Resource(Device const* device) : device(device) {}
	virtual ~Resource() = default;
};
}// namespace toolhub::vk