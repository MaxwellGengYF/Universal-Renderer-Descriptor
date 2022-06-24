#include "device.h"
#include <Common/small_vector.h>
#include <vulkan_impl/gpu_allocator/gpu_allocator.h>
namespace toolhub::vk {
namespace detail {
class VulkanAllocatorImpl {
public:
	VkAllocationCallbacks allocator;
	VulkanAllocatorImpl() {
		allocator.pfnAllocation =
			[](void* pUserData,
			   size_t size,
			   size_t alignment,
			   VkSystemAllocationScope allocationScope) {
				return vengine_malloc(size);
			};
		allocator.pfnFree =
			[](void* pUserData,
			   void* pMemory) {
				vengine_free(pMemory);
			};
		allocator.pfnReallocation =
			[](void* pUserData,
			   void* pOriginal,
			   size_t size,
			   size_t alignment,
			   VkSystemAllocationScope allocationScope) {
				return vengine_realloc(pOriginal, size);
			};
		allocator.pfnInternalAllocation =
			[](void* pUserData,
			   size_t size,
			   VkInternalAllocationType allocationType,
			   VkSystemAllocationScope allocationScope) {

			};
		allocator.pfnInternalFree =
			[](void* pUserData,
			   size_t size,
			   VkInternalAllocationType allocationType,
			   VkSystemAllocationScope allocationScope) {};
	}
};
static VulkanAllocatorImpl gVulkanAllocatorImpl;
}// namespace detail
VkAllocationCallbacks* Device::Allocator() {
	return &detail::gVulkanAllocatorImpl.allocator;
}

Device::Device() {
}
void Device::Init() {
	psoHeader.Init(this);
	gpuAllocator = vstd::create_unique(new GPUAllocator(this));
	vkGetAccelerationStructureBuildSizesKHR = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(vkGetDeviceProcAddr(device, "vkGetAccelerationStructureBuildSizesKHR"));
	vkCreateAccelerationStructureKHR = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(vkGetDeviceProcAddr(device, "vkCreateAccelerationStructureKHR"));
	vkDestroyAccelerationStructureKHR = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(vkGetDeviceProcAddr(device, "vkDestroyAccelerationStructureKHR"));
	vkCmdBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(vkGetDeviceProcAddr(device, "vkCmdBuildAccelerationStructuresKHR"));
	vkGetAccelerationStructureDeviceAddressKHR = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(vkGetDeviceProcAddr(device, "vkGetAccelerationStructureDeviceAddressKHR"));
}
Device::~Device() {
	gpuAllocator = nullptr;
	vkDestroyDevice(device, Device::Allocator());
}

//TODO
Device* Device::CreateDevice(
	VkInstance instance,
	VkSurfaceKHR surface,
	vstd::span<char const* const> validationLayers,
	uint physicalDeviceIndex,
	void* placedMemory) {
	std::initializer_list<char const*> requiredFeatures = {
		"VK_EXT_descriptor_indexing",
		"VK_KHR_buffer_device_address",
		"VK_KHR_deferred_host_operations",
		"VK_KHR_acceleration_structure",
		"VK_KHR_ray_query"};
	auto checkDeviceExtensionSupport = [&](VkPhysicalDevice device) {
		uint32_t extensionCount;
		ThrowIfFailed(vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr));

		vstd::small_vector<VkExtensionProperties> availableExtensions(extensionCount);
		ThrowIfFailed(vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data()));
		vstd::HashMap<vstd::string_view> requiredExtensions(requiredFeatures.size());
		for (auto&& i : requiredFeatures) {
			requiredExtensions.Emplace(i);
		}
		size_t supportedExt = 0;
		for (const auto& extension : availableExtensions) {
			char const* ptr = extension.extensionName;
			if (requiredExtensions.Find(ptr)) {
				supportedExt++;
			}
		}
		return supportedExt == requiredExtensions.Size();
	};
	auto isDeviceSuitable = [&](VkPhysicalDevice device) {
		VkPhysicalDeviceProperties deviceProperties;
		VkPhysicalDeviceFeatures deviceFeatures;
		vkGetPhysicalDeviceProperties(device, &deviceProperties);
		vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

		if (deviceProperties.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
			return false;
		bool extensionsSupported = checkDeviceExtensionSupport(device);
		//TODO: probably need other checks, like swapchain, etc
		return extensionsSupported;
	};
	uint deviceCount = 0;
	ThrowIfFailed(vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr));
	if (deviceCount == 0) {
		return nullptr;
	}

	vstd::small_vector<VkPhysicalDevice> devices(deviceCount);
	ThrowIfFailed(vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data()));
	auto physicalDevice = devices[std::min<uint>(physicalDeviceIndex, deviceCount - 1)];
	if (!isDeviceSuitable(physicalDevice))
		return nullptr;
	vstd::optional<uint32_t> computeFamily;
	vstd::optional<uint32_t> presentFamily;

	auto findQueueFamilies = [&]() {
		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

		vstd::small_vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());
		int i = 0;
		for (const auto& queueFamily : queueFamilies) {
			if (!computeFamily && queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) {
				computeFamily = i;
			}
			if (surface) {
				VkBool32 presentSupport = false;
				vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport);
				if (presentSupport) {
					presentFamily = i;
				}
			}
			++i;
		}
	};
	findQueueFamilies();
	vstd::small_vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	vstd::optional<uint> uniqueQueueFamilies[] = {
		computeFamily,
		presentFamily};
	float queuePriority = 1.0f;
	for (auto&& queueFamily : uniqueQueueFamilies) {
		if (!queueFamily.has_value()) continue;
		VkDeviceQueueCreateInfo queueCreateInfo{};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = *queueFamily;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		queueCreateInfos.push_back(queueCreateInfo);
	}
	//check bindless

	/*VkPhysicalDeviceDescriptorIndexingFeaturesEXT indexingFeatures{};
	indexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT;
	indexingFeatures.pNext = nullptr;
	VkPhysicalDeviceFeatures2 deviceFeatures{};
	deviceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	deviceFeatures.pNext = &indexingFeatures;
	vkGetPhysicalDeviceFeatures2(physicalDevice, &deviceFeatures);
	//if not support bindless, return
	if (!indexingFeatures.runtimeDescriptorArray
		|| !indexingFeatures.descriptorBindingVariableDescriptorCount
		|| !indexingFeatures.shaderSampledImageArrayNonUniformIndexing) {
		return nullptr;
	}*/

	VkPhysicalDeviceFeatures deviceFeatures{};
	vkGetPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);
	//bindless
	VkPhysicalDeviceDescriptorIndexingFeaturesEXT indexingFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT};
	indexingFeatures.runtimeDescriptorArray = VK_TRUE;
	indexingFeatures.descriptorBindingVariableDescriptorCount = VK_TRUE;
	indexingFeatures.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
	VkPhysicalDeviceBufferDeviceAddressFeatures enabledBufferDeviceAddresFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES};
	enabledBufferDeviceAddresFeatures.bufferDeviceAddress = VK_TRUE;
	enabledBufferDeviceAddresFeatures.pNext = &indexingFeatures;

	VkPhysicalDeviceRayTracingPipelineFeaturesKHR enabledRayTracingPipelineFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR};
	enabledRayTracingPipelineFeatures.rayTracingPipeline = VK_TRUE;
	enabledRayTracingPipelineFeatures.pNext = &enabledBufferDeviceAddresFeatures;

	VkPhysicalDeviceAccelerationStructureFeaturesKHR enabledAccelerationStructureFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};
	enabledAccelerationStructureFeatures.accelerationStructure = VK_TRUE;
	enabledAccelerationStructureFeatures.pNext = &enabledRayTracingPipelineFeatures;

	VkPhysicalDeviceRayQueryFeaturesKHR enabledRayQueryFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR};
	enabledRayQueryFeatures.rayQuery = VK_TRUE;
	enabledRayQueryFeatures.pNext = &enabledAccelerationStructureFeatures;
	auto featureLinkQueue = &enabledRayQueryFeatures;

	VkDeviceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.pNext = featureLinkQueue;
	createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	createInfo.pQueueCreateInfos = queueCreateInfos.data();
	createInfo.pEnabledFeatures = &deviceFeatures;
	createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredFeatures.size());
	createInfo.ppEnabledExtensionNames = requiredFeatures.begin();
	createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
	createInfo.ppEnabledLayerNames = validationLayers.data();
	VkDevice device;
	if (vkCreateDevice(physicalDevice, &createInfo, Device::Allocator(), &device) != VK_SUCCESS) {
		return nullptr;
	}
	if (!computeFamily) return nullptr;
	Device* result = placedMemory ? new (placedMemory) Device() : new Device();
	if (computeFamily) {
		vkGetDeviceQueue(device, *computeFamily, 0, &result->computeQueue);
		result->computeFamily = *computeFamily;
	}
	if (presentFamily) {
		vkGetDeviceQueue(device, *presentFamily, 0, &result->presentQueue);
		result->presentFamily = presentFamily;
	} else {
		result->presentQueue = nullptr;
	}
	result->physicalDevice = physicalDevice;
	result->device = device;
	result->instance = instance;
	vkGetPhysicalDeviceProperties(physicalDevice, &result->deviceProperties);
	result->Init();
	return result;
}
void PipelineCachePrefixHeader::Init(Device const* device) {
	magic = PSO_MAGIC_NUM;
	vendorID = device->deviceProperties.vendorID;
	deviceID = device->deviceProperties.deviceID;
	driverVersion = device->deviceProperties.driverVersion;
	memcpy(uuid, device->deviceProperties.pipelineCacheUUID, VK_UUID_SIZE);
}
bool PipelineCachePrefixHeader::operator==(PipelineCachePrefixHeader const& v) const {
	return memcmp(this, &v, sizeof(PipelineCachePrefixHeader)) == 0;
}
}// namespace toolhub::vk