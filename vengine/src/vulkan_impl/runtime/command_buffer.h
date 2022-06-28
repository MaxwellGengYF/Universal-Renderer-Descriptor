#pragma once
#include <vulkan_impl/components/resource.h>
#include <vulkan_impl/types/bind_desriptor.h>
#include <Utility/StackAllocator.h>
namespace toolhub::vk {
class FrameResource;
class ComputeShader;
class Buffer;
class DescriptorSetManager;
class ResStateTracker;
class CommandBuffer : public Resource {
	friend class FrameResource;
	VkCommandBuffer cmdBuffer;
	FrameResource* pool;
	DescriptorSetManager* descManager;
	CommandBuffer(
		DescriptorSetManager* descManager,
		Device const* device,
		VkCommandBuffer cmdBuffer,
		FrameResource* pool);

public:
	VkCommandBuffer CmdBuffer() const { return cmdBuffer; }
	CommandBuffer(CommandBuffer const&) = delete;
	CommandBuffer(CommandBuffer&& v)
		: cmdBuffer(v.cmdBuffer), pool(v.pool), descManager(v.descManager), Resource(v) {
		v.cmdBuffer = nullptr;
	}
	~CommandBuffer();
	void PreprocessCopyBuffer(
		ResStateTracker& stateTracker,
		Buffer const* srcBuffer,
		size_t srcOffset,
		Buffer const* dstBuffer,
		size_t dstOffset,
		size_t size);
	void CopyBuffer(
		Buffer const* srcBuffer,
		size_t srcOffset,
		Buffer const* dstBuffer,
		size_t dstOffset,
		size_t size);
	VkDescriptorSet PreprocessDispatch(
		ComputeShader const* cs,
		ResStateTracker& stateTracker,
		vstd::span<BindResource const> binds);
	void Dispatch(
		VkDescriptorSet set,
		ComputeShader const* cs,
		uint3 dispatchCount);
};
}// namespace toolhub::vk