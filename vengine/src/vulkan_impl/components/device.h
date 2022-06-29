#pragma once
#include <vulkan_impl/vulkan_include.h>
#include <Utility/StackAllocator.h>
namespace toolhub::vk {
static constexpr uint PSO_MAGIC_NUM = 2595790981u;
class Device;
class DescriptorSetManager;
class DescriptorPool;
class GPUAllocator;
class TexView;
class BufferView;

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

class Device : public vstd::IOperatorNewBase {
	Device();
	void Init();
	void InitBindless();
	mutable vstd::spin_mutex updateBindlessMtx;
	mutable vstd::spin_mutex allocIdxMtx;
	mutable vstd::vector<uint16> bindlessIdx;
	mutable vstd::vector<VkWriteDescriptorSet> bindlessWriteRes;
	mutable vstd::StackAllocator bindlessStackAlloc;

public:
	VkPhysicalDeviceLimits limits;
	static VkInstance InitVkInstance();
	uint16 AllocateBindlessIdx() const;
	void DeAllocateBindlessIdx(uint16 index) const;
	mutable vstd::DefaultMallocVisitor mallocVisitor;
	vstd::unique_ptr<DescriptorSetManager> manager;
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
		uint physicalDeviceIndex,
		void* placedMemory = nullptr);
	~Device();
	//KHR functions
	PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR;
	PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR;
	PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR;
	PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR;
	PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR;
	PFN_vkCmdWriteAccelerationStructuresPropertiesKHR vkCmdWriteAccelerationStructuresPropertiesKHR;
	PFN_vkCmdCopyAccelerationStructureKHR vkCmdCopyAccelerationStructureKHR;
	vstd::unique_ptr<DescriptorPool> pool;
	VkDescriptorSetLayout samplerSetLayout;
	VkDescriptorSet samplerSet;
	VkDescriptorSetLayout bindlessTex2DSetLayout;
	VkDescriptorSet bindlessTex2DSet;
	VkDescriptorSetLayout bindlessTex3DSetLayout;
	VkDescriptorSet bindlessTex3DSet;
	VkDescriptorSetLayout bindlessBufferSetLayout;
	VkDescriptorSet bindlessBufferSet;
	std::array<VkSampler, 16> samplers;
	void AddBindlessBufferUpdateCmd(size_t index, BufferView const& buffer) const;
	void AddBindlessTex2DUpdateCmd(size_t index, TexView const& tex) const;
	void AddBindlessTex3DUpdateCmd(size_t index, TexView const& tex) const;
	void UpdateBindless() const;
};
}// namespace toolhub::vk