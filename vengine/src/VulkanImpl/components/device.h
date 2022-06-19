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
class GPUAllocator;
class Device : public vstd::IOperatorNewBase {
	Device();
	void Init();

public:
	vstd::unique_ptr<GPUAllocator> gpuAllocator;
	VkQueue computeQueue;
	VkQueue presentQueue;
	uint computeFamily;
	vstd::optional<uint> presentFamily;
	VkInstance instance;
	VkPhysicalDevice physicalDevice;
	VkDevice device;
	VkPhysicalDeviceProperties deviceProperties;
	PipelineCachePrefixHeader psoHeader;
	static VkAllocationCallbacks* Allocator();
	static Device* CreateDevice(
		VkInstance instance,
		VkSurfaceKHR surface,
		vstd::span<char const* const> requiredFeatures,
		vstd::span<char const* const> validationLayers,
		uint physicalDeviceIndex,
		void* placedMemory = nullptr);
	~Device();
};
}// namespace toolhub::vk