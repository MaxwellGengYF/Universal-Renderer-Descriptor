#include "command_pool.h"
#include "command_buffer.h"
#include "frame_resource.h"
#include <vulkan_impl/gpu_collection/buffer.h>
#include <vulkan_impl/shader/descriptorset_manager.h>
#include <vulkan_impl/shader/compute_shader.h>
#include <vulkan_impl/runtime/res_state_tracker.h>
#include <vulkan_impl/rtx/accel.h>
namespace toolhub::vk {
CommandBuffer::~CommandBuffer() {
	if (cmdBuffer) {
		ThrowIfFailed(vkEndCommandBuffer(cmdBuffer));
		pool->ReleaseCmdBuffer(cmdBuffer);
	}
}
CommandBuffer::CommandBuffer(
	DescriptorSetManager* descManager,
	Device const* device,
	VkCommandBuffer cmdBuffer,
	FrameResource* pool)
	: Resource(device),
	  descManager(descManager),
	  pool(pool), cmdBuffer(cmdBuffer) {
	VkCommandBufferBeginInfo info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
	ThrowIfFailed(vkResetCommandBuffer(cmdBuffer, 0));
	ThrowIfFailed(vkBeginCommandBuffer(cmdBuffer, &info));
}
void CommandBuffer::PreprocessCopyBuffer(
	ResStateTracker& stateTracker,
	Buffer const* srcBuffer,
	size_t srcOffset,
	Buffer const* dstBuffer,
	size_t dstOffset,
	size_t size) {
	stateTracker.MarkBufferRead(
		BufferView(srcBuffer, srcOffset, size),
		BufferReadState::ComputeOrCopy);
	stateTracker.MarkBufferWrite(
		BufferView(dstBuffer, dstOffset, size),
		BufferWriteState::Copy);
}
void CommandBuffer::CopyBuffer(
	Buffer const* srcBuffer,
	size_t srcOffset,
	Buffer const* dstBuffer,
	size_t dstOffset,
	size_t size) {
	VkBufferCopy copyRegion{srcOffset, dstOffset, size};

	vkCmdCopyBuffer(cmdBuffer, srcBuffer->GetResource(), dstBuffer->GetResource(), 1, &copyRegion);
}
void CommandBuffer::PreprocessDispatch(
	ResStateTracker& stateTracker,
	vstd::span<BindResource const> binds) {
	for (auto&& desc : binds) {
		desc.res.multi_visit(
			[&](Texture const* t) {
				if (desc.writable) {
					stateTracker.MarkTextureWrite(
						t, desc.offset, TextureWriteState::Compute);
				} else {
					stateTracker.MarkTextureRead(TexView(t, desc.offset, desc.size));
				}
			},
			[&](Buffer const* b) {
				auto bfView = BufferView(b, desc.offset, desc.size);
				if (desc.writable) {
					stateTracker.MarkBufferWrite(
						bfView,
						BufferWriteState::Compute);
				} else {
					stateTracker.MarkBufferRead(
						bfView,
						BufferReadState::ComputeOrCopy);
				}
			},
			[&](Accel const* a) {
				stateTracker.MarkBufferRead(
					a->AccelBuffer(),
					BufferReadState::UseAccel);
			});
	}
}
void CommandBuffer::Dispatch(
	ComputeShader const* cs,
	vstd::span<BindResource const> binds,
	uint3 dispatchCount) {
	vkCmdBindPipeline(
		cmdBuffer,
		VK_PIPELINE_BIND_POINT_COMPUTE,
		cs->pipeline);
	auto set = descManager->Allocate(
		cs->descriptorSetLayout,
		cs->propertiesTypes,
		binds);
	VkDescriptorSet sets[] = {
		set,
		device->bindlessBufferSet,
		device->bindlessTexSet,
		device->samplerSet};
	vkCmdBindDescriptorSets(
		cmdBuffer,
		VK_PIPELINE_BIND_POINT_COMPUTE,
		cs->pipelineLayout,
		0, vstd::array_count(sets), sets,
		0, nullptr);
	vkCmdDispatch(
		cmdBuffer,
		(dispatchCount.x + (cs->threadGroupSize.x - 1)) / cs->threadGroupSize.x,
		(dispatchCount.y + (cs->threadGroupSize.y - 1)) / cs->threadGroupSize.y,
		(dispatchCount.z + (cs->threadGroupSize.z - 1)) / cs->threadGroupSize.z);
}
}// namespace toolhub::vk