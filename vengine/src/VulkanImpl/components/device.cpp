#include <components/device.h>
#include <Common/small_vector.h>
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
	psoHeader.Init(this);
}
Device::~Device() {}

//TODO
Device* Device::CreateDevice(
	VkInstance instance,
	VkSurfaceKHR surface,
	vstd::span<char const*> requiredFeatures,
	vstd::span<char const*> validationLayers,
	uint physicalDeviceIndex,
	void* placedMemory) {
	auto checkDeviceExtensionSupport = [&](VkPhysicalDevice device) {
		uint32_t extensionCount;
		ThrowIfFailed(vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr));

		vstd::small_vector<VkExtensionProperties> availableExtensions(extensionCount);
		ThrowIfFailed(vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data()));
		vstd::HashMap<vstd::string_view> requiredExtensions(requiredFeatures.size());
		for (auto&& i : requiredFeatures) {
			requiredExtensions.Emplace(i);
		}

		for (const auto& extension : availableExtensions) {
			if (!requiredExtensions.Find(extension.extensionName)) return false;
		}
		return true;
	};
	auto isDeviceSuitable = [&](VkPhysicalDevice device) {
		VkPhysicalDeviceProperties deviceProperties;
		VkPhysicalDeviceFeatures deviceFeatures;
		vkGetPhysicalDeviceProperties(device, &deviceProperties);
		vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

		if (!(deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && deviceFeatures.geometryShader)) return false;
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
	if (!isDeviceSuitable(physicalDevice)) return nullptr;
	vstd::optional<uint32_t> computeFamily;
	vstd::optional<uint32_t> presentFamily;

	auto findQueueFamilies = [&]() {
		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

		vstd::small_vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());
		int i = 0;
		for (const auto& queueFamily : queueFamilies) {
			if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) {
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

	VkPhysicalDeviceFeatures deviceFeatures{};
	VkDeviceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	createInfo.pQueueCreateInfos = queueCreateInfos.data();
	createInfo.pEnabledFeatures = &deviceFeatures;
	createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredFeatures.size());
	createInfo.ppEnabledExtensionNames = requiredFeatures.data();
	createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
	createInfo.ppEnabledLayerNames = validationLayers.data();
	VkDevice device;
	if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
		return nullptr;
	}
	Device* result = placedMemory ? new (placedMemory) Device() : new Device();
	result->computeFamily = computeFamily;
	result->presentFamily = presentFamily;
	result->physicalDevice = physicalDevice;
	result->device = device;
	vkGetPhysicalDeviceProperties(physicalDevice, &result->deviceProperties);
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