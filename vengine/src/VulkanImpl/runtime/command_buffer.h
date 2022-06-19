#pragma once
#include <components/resource.h>
#include <types/bind_desriptor.h>
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
	CommandBuffer(
		Device const* device,
		VkCommandBuffer cmdBuffer,
		FrameResource* pool);

public:
	VkCommandBuffer CmdBuffer() const { return cmdBuffer; }
	CommandBuffer(CommandBuffer const&) = delete;
	CommandBuffer(CommandBuffer&& v)
		: cmdBuffer(v.cmdBuffer), pool(v.pool), Resource(v) {
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
	void PreprocessDispatch(
		ResStateTracker& stateTracker,
		vstd::span<BindResource const> binds);
	void Dispatch(
		ComputeShader const* cs,
		DescriptorSetManager* descManager,
		vstd::span<BindResource const> binds,
		uint3 dispatchCount);
};
}// namespace toolhub::vk