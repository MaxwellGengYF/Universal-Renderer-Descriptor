#pragma once
#include <vulkan_include.h>
namespace toolhub::vk {
static constexpr uint PSO_MAGIC_NUM = 2595790981u;
class Device;
struct PipelineCachePrefixHeader {
	uint magic;				 // an arbitrary magic header to make sure this is actually our file
	uint vendorID;			 // equal to VkPhysicalDeviceProperties::vendorID
	uint deviceID;			 // equal to VkPhysicalDeviceProperties::deviceID
	uint driverVersion;		 // equal to VkPhysicalDeviceProperties::driverVersion
	vbyte uuid[VK_UUID_SIZE];// equal to VkPhysicalDeviceProperties::pipelineCacheUUID
	void Init(Device const* device);
	bool operator==(PipelineCachePrefixHeader const& v) const;
	bool operator!=(PipelineCachePrefixHeader const& v) { return !operator==(v); }
};
class Device {
public:
	VkDevice device;
	VkAllocationCallbacks allocator;
	VkPhysicalDeviceProperties deviceProperties;
    PipelineCachePrefixHeader psoHeader;
	Device();
	~Device();
};
}// namespace toolhub::vk