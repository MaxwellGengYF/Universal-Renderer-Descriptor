#include <components/gpu_allocator/gpu_allocator.h>
namespace toolhub::vk {
namespace GpuAllocDetail {
static bool VK_KHR_dedicated_allocation_enabled = false;
static bool VK_KHR_bind_memory2_enabled = false;
static bool VK_KHR_get_physical_device_properties2_enabled = false;
static bool VK_EXT_memory_budget_enabled = false;
static bool VK_AMD_device_coherent_memory_enabled = false;
static bool VK_KHR_buffer_device_address_enabled = false;
static bool VK_EXT_memory_priority_enabled = false;
static const bool USE_CUSTOM_CPU_ALLOCATION_CALLBACKS = true;

static void SetAllocatorCreateInfo(
	VkInstance g_hVulkanInstance,
	VkPhysicalDevice g_hPhysicalDevice,
	VkDevice g_hDevice,
	VmaAllocatorCreateInfo& outInfo) {
	outInfo = {};

	outInfo.physicalDevice = g_hPhysicalDevice;
	outInfo.device = g_hDevice;
	outInfo.instance = g_hVulkanInstance;
	outInfo.vulkanApiVersion = VulkanApiVersion;

	if (VK_KHR_dedicated_allocation_enabled) {
		outInfo.flags |= VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT;
	}
	if (VK_KHR_bind_memory2_enabled) {
		outInfo.flags |= VMA_ALLOCATOR_CREATE_KHR_BIND_MEMORY2_BIT;
	}
#if !defined(VMA_MEMORY_BUDGET) || VMA_MEMORY_BUDGET == 1
	if (VK_EXT_memory_budget_enabled && (VulkanApiVersion >= VK_API_VERSION_1_1 || VK_KHR_get_physical_device_properties2_enabled)) {
		outInfo.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
	}
#endif
	if (VK_AMD_device_coherent_memory_enabled) {
		outInfo.flags |= VMA_ALLOCATOR_CREATE_AMD_DEVICE_COHERENT_MEMORY_BIT;
	}
	if (VK_KHR_buffer_device_address_enabled) {
		outInfo.flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
	}
#if !defined(VMA_MEMORY_PRIORITY) || VMA_MEMORY_PRIORITY == 1
	if (VK_EXT_memory_priority_enabled) {
		outInfo.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_PRIORITY_BIT;
	}
#endif

	if (USE_CUSTOM_CPU_ALLOCATION_CALLBACKS) {
		outInfo.pAllocationCallbacks = Device::Allocator();
	}

#if VMA_DYNAMIC_VULKAN_FUNCTIONS
	static VmaVulkanFunctions vulkanFunctions = {};
	vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
	vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
	outInfo.pVulkanFunctions = &vulkanFunctions;
#endif

	// Uncomment to enable recording to CSV file.
	/*
    static VmaRecordSettings recordSettings = {};
    recordSettings.pFilePath = "VulkanSample.csv";
    outInfo.pRecordSettings = &recordSettings;
    */

	// Uncomment to enable HeapSizeLimit.
	/*
    static std::array<VkDeviceSize, VK_MAX_MEMORY_HEAPS> heapSizeLimit;
    std::fill(heapSizeLimit.begin(), heapSizeLimit.end(), VK_WHOLE_SIZE);
    heapSizeLimit[0] = 512ull * 1024 * 1024;
    outInfo.pHeapSizeLimit = heapSizeLimit.data();
    */
}
}// namespace GpuAllocDetail
GPUAllocator::GPUAllocator(Device const* device)
	: Resource(device) {
	using namespace GpuAllocDetail;
	VmaAllocatorCreateInfo allocatorInfo = {};
	SetAllocatorCreateInfo(
		device->instance,
		device->physicalDevice,
		device->device,
		allocatorInfo);
	ThrowIfFailed(vmaCreateAllocator(&allocatorInfo, &allocator));
}
GPUAllocator::~GPUAllocator() {
	vmaDestroyAllocator(allocator);
}
VmaBuffer::VmaBuffer(
	GPUAllocator& alloc,
	size_t byteSize,
	VkBufferUsageFlagBits usage,
	bool crossQueueShared,
	RWState hostRW) {
	allocator = alloc.allocator;
	VkBufferCreateInfo vbInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
	vbInfo.size = byteSize;
	vbInfo.usage = usage;
	vbInfo.sharingMode = crossQueueShared ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo vbAllocCreateInfo = {};
	vbAllocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
	switch (hostRW) {
		case RWState::Upload:
			vbAllocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
			break;
		case RWState::Readback:
			vbAllocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
			break;
	}
	vbAllocCreateInfo.flags |= VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
	VmaAllocationInfo allocInfo;
	vmaCreateBuffer(
		alloc.allocator,
		&vbInfo,
		&vbAllocCreateInfo,
		&buffer,
		&this->alloc,
		&allocInfo);
	mappedPtr = allocInfo.pMappedData;
}
VmaBuffer::~VmaBuffer() {
	vmaDestroyBuffer(
		allocator,
		buffer,
		alloc);
}
}// namespace toolhub::vk