#include "device.h"
#include <Common/small_vector.h>
#include <vulkan_impl/gpu_allocator/gpu_allocator.h>
#include <vulkan_impl/shader/descriptorset_manager.h>
#include <vulkan_impl/types/buffer_view.h>
#include <vulkan_impl/types/tex_view.h>
#include <vulkan_impl/gpu_collection/buffer.h>
#include <vulkan_impl/gpu_collection/texture.h>
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

Device::Device()
	: bindlessStackAlloc(4096, &mallocVisitor) {
}
void Device::Init() {
	psoHeader.Init(this);
	gpuAllocator = vstd::create_unique(new GPUAllocator(this));
	manager = vstd::create_unique(new DescriptorSetManager(this));
	// create sampler desc-set layout
	VkDescriptorSetLayoutBinding binding =
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 0, DescriptorPool::MAX_SAMP);
	VkDescriptorSetLayoutCreateInfo descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo({&binding, 1});
	ThrowIfFailed(vkCreateDescriptorSetLayout(device, &descriptorLayout, Device::Allocator(), &samplerSetLayout));
	pool = vstd::create_unique(new DescriptorPool(this));
	samplerSet = pool->Allocate(samplerSetLayout);
	VkFilter filters[4] = {
		VK_FILTER_NEAREST,
		VK_FILTER_LINEAR,
		VK_FILTER_LINEAR,
		VK_FILTER_LINEAR};
	VkSamplerMipmapMode mipFilter[4] = {
		VK_SAMPLER_MIPMAP_MODE_NEAREST,
		VK_SAMPLER_MIPMAP_MODE_NEAREST,
		VK_SAMPLER_MIPMAP_MODE_LINEAR,
		VK_SAMPLER_MIPMAP_MODE_LINEAR};
	VkSamplerAddressMode addressMode[4] = {
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		VK_SAMPLER_ADDRESS_MODE_REPEAT,
		VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER};

	size_t idx = 0;
	VkDescriptorImageInfo imageInfos[DescriptorPool::MAX_SAMP];
	for (auto x : vstd::range(4))
		for (auto y : vstd::range(4)) {
			auto d = vstd::create_disposer([&] { ++idx; });
			auto&& samp = samplers[idx];
			VkSamplerCreateInfo createInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
			createInfo.magFilter = filters[y];
			createInfo.minFilter = filters[y];
			createInfo.mipmapMode = mipFilter[y];
			createInfo.addressModeU = addressMode[x];
			createInfo.addressModeV = addressMode[x];
			createInfo.addressModeW = addressMode[x];
			createInfo.mipLodBias = 0;
			createInfo.anisotropyEnable = y == 3;
			createInfo.maxAnisotropy = DescriptorPool::MAX_SAMP;
			createInfo.minLod = 0;
			createInfo.maxLod = 255;
			vkCreateSampler(
				device,
				&createInfo,
				Device::Allocator(),
				&samp);
			imageInfos[idx].sampler = samp;
		}
	VkWriteDescriptorSet writeDesc{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
	writeDesc.dstSet = samplerSet;
	writeDesc.dstBinding = 0;
	writeDesc.dstArrayElement = 0;
	writeDesc.descriptorCount = DescriptorPool::MAX_SAMP;
	writeDesc.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
	writeDesc.pImageInfo = imageInfos;
	vkUpdateDescriptorSets(device, 1, &writeDesc, 0, nullptr);
	InitBindless();

	vkGetAccelerationStructureBuildSizesKHR = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(vkGetDeviceProcAddr(device, "vkGetAccelerationStructureBuildSizesKHR"));
	vkCreateAccelerationStructureKHR = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(vkGetDeviceProcAddr(device, "vkCreateAccelerationStructureKHR"));
	vkDestroyAccelerationStructureKHR = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(vkGetDeviceProcAddr(device, "vkDestroyAccelerationStructureKHR"));
	vkCmdBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(vkGetDeviceProcAddr(device, "vkCmdBuildAccelerationStructuresKHR"));
	vkGetAccelerationStructureDeviceAddressKHR = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(vkGetDeviceProcAddr(device, "vkGetAccelerationStructureDeviceAddressKHR"));
	vkCmdWriteAccelerationStructuresPropertiesKHR = reinterpret_cast<PFN_vkCmdWriteAccelerationStructuresPropertiesKHR>(vkGetDeviceProcAddr(device, "vkCmdWriteAccelerationStructuresPropertiesKHR"));
	vkCmdCopyAccelerationStructureKHR = reinterpret_cast<PFN_vkCmdCopyAccelerationStructureKHR>(vkGetDeviceProcAddr(device, "vkCmdCopyAccelerationStructureKHR"));
}
void Device::InitBindless() {
	VkDescriptorSetLayoutBinding setLayoutBinding{};
	setLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	setLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	setLayoutBinding.binding = 0;
	setLayoutBinding.descriptorCount = DescriptorPool::MAX_BINDLESS_SIZE;
	VkDescriptorSetVariableDescriptorCountAllocateInfo bindlessSize{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO};
	bindlessSize.pDescriptorCounts = &setLayoutBinding.descriptorCount;
	bindlessSize.descriptorSetCount = 1;

	VkDescriptorSetLayoutCreateInfo descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo({&setLayoutBinding, 1});
	descriptorLayout.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
	VkDescriptorSetLayoutBindingFlagsCreateInfoEXT setLayoutBindingFlags{};
	setLayoutBindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT;
	setLayoutBindingFlags.bindingCount = 1;
	VkDescriptorBindingFlagsEXT descriptorBindingFlags =
		VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT
		| VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
		| VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
		| VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT;
	setLayoutBindingFlags.pBindingFlags = &descriptorBindingFlags;
	descriptorLayout.pNext = &setLayoutBindingFlags;
	ThrowIfFailed(vkCreateDescriptorSetLayout(device, &descriptorLayout, Device::Allocator(), &bindlessTexSetLayout));
	bindlessTexSet = pool->Allocate(bindlessTexSetLayout, &bindlessSize);
	setLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	ThrowIfFailed(vkCreateDescriptorSetLayout(device, &descriptorLayout, Device::Allocator(), &bindlessBufferSetLayout));
	bindlessBufferSet = pool->Allocate(bindlessBufferSetLayout, &bindlessSize);
	bindlessIdx.push_back_func(DescriptorPool::MAX_BINDLESS_SIZE, [](size_t i) { return i; });
}
uint16 Device::AllocateBindlessIdx() const {
	std::lock_guard lck(allocIdxMtx);
	return bindlessIdx.erase_last();
}
void Device::DeAllocateBindlessIdx(uint16 index) const {
	std::lock_guard lck(allocIdxMtx);
	bindlessIdx.emplace_back(index);
}
Device::~Device() {
	pool->Destroy(samplerSet);
	pool->Destroy(bindlessTexSet);
	pool->Destroy(bindlessBufferSet);
	vkDestroyDescriptorSetLayout(
		device,
		bindlessTexSetLayout,
		Device::Allocator());
	vkDestroyDescriptorSetLayout(
		device,
		bindlessBufferSetLayout,
		Device::Allocator());
	vkDestroyDescriptorSetLayout(
		device,
		samplerSetLayout,
		Device::Allocator());
	for (auto&& i : samplers) {
		vkDestroySampler(device, i, Device::Allocator());
	}

	gpuAllocator = nullptr;
	pool = nullptr;
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
		"VK_EXT_host_query_reset",
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
	VkPhysicalDeviceDescriptorIndexingFeatures indexingFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT};
	indexingFeatures.runtimeDescriptorArray = VK_TRUE;
	indexingFeatures.descriptorBindingVariableDescriptorCount = VK_TRUE;
	indexingFeatures.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
	indexingFeatures.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
	indexingFeatures.descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE;
	indexingFeatures.descriptorBindingUpdateUnusedWhilePending = VK_TRUE;
	indexingFeatures.descriptorBindingPartiallyBound = VK_TRUE;
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

	VkPhysicalDeviceHostQueryResetFeatures enableQueryReset{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES};
	enableQueryReset.hostQueryReset = VK_TRUE;
	enableQueryReset.pNext = &enabledRayQueryFeatures;

	auto featureLinkQueue = &enableQueryReset;

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
void Device::AddBindlessUpdateCmd(size_t index, BufferView const& buffer) const{
	std::lock_guard lck(updateBindlessMtx);
	auto&& writeDesc = bindlessWriteRes.emplace_back();
	memset(&writeDesc, 0, sizeof(VkWriteDescriptorSet));
	writeDesc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeDesc.dstSet = bindlessBufferSet;
	writeDesc.dstBinding = 0;
	writeDesc.dstArrayElement = index;
	writeDesc.descriptorCount = 1;
	writeDesc.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	auto bf = bindlessStackAlloc.Allocate(sizeof(VkDescriptorBufferInfo));
	auto ptr = reinterpret_cast<VkDescriptorBufferInfo*>(bf.handle + bf.offset);
	*ptr = buffer.buffer->GetDescriptor(
		buffer.offset,
		buffer.size);
	writeDesc.pBufferInfo = ptr;
}
void Device::AddBindlessUpdateCmd(size_t index, TexView const& tex) const{
	std::lock_guard lck(updateBindlessMtx);
	auto&& writeDesc = bindlessWriteRes.emplace_back();
	memset(&writeDesc, 0, sizeof(VkWriteDescriptorSet));
	writeDesc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeDesc.dstSet = bindlessTexSet;
	writeDesc.dstBinding = 0;
	writeDesc.dstArrayElement = index;
	writeDesc.descriptorCount = 1;
	writeDesc.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	auto bf = bindlessStackAlloc.Allocate(sizeof(VkDescriptorImageInfo));
	auto ptr = reinterpret_cast<VkDescriptorImageInfo*>(bf.handle + bf.offset);
	*ptr = tex.tex->GetDescriptor(
		tex.mipOffset,
		tex.mipCount);
	writeDesc.pImageInfo = ptr;
}

void Device::UpdateBindless() const{
	std::lock_guard lck(updateBindlessMtx);
	vkUpdateDescriptorSets(device, bindlessWriteRes.size(), bindlessWriteRes.data(), 0, nullptr);
	bindlessWriteRes.clear();
	bindlessStackAlloc.Clear();
}
}// namespace toolhub::vk