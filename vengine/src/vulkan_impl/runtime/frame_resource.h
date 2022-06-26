#pragma once
#include <vulkan_impl/components/resource.h>
#include "command_buffer.h"
#include <Utility/StackAllocator.h>
#include <vulkan_impl/gpu_collection/buffer.h>
#include <vulkan_impl/shader/descriptorset_manager.h>
namespace toolhub::vk {
class CommandPool;
class Event;
template<RWState state>
struct BufferStackVisitor : public vstd::StackAllocatorVisitor {
	Device const* device;
	uint64 Allocate(uint64 size);
	void DeAllocate(uint64 handle);
};
class FrameResource : public Resource {
	friend class CommandBuffer;
	vstd::small_vector<VkCommandBuffer> allocatedBuffers;
	vstd::small_vector<VkCommandBuffer> needExecuteBuffers;
	BufferStackVisitor<RWState::Upload> uploadVisitor;
	BufferStackVisitor<RWState::None> defaultVisitor;
	BufferStackVisitor<RWState::Readback> readbackVisitor;
	vstd::StackAllocator uploadAlloc;
	vstd::StackAllocator defaultAlloc;
	vstd::StackAllocator readBackAlloc;
	CommandPool* pool;
	VkFence syncFence;
	VkSemaphore semaphore;
	DescriptorSetManager descManager;
	bool executing = false;
	void ReleaseCmdBuffer(VkCommandBuffer buffer);
	struct CopyBuffer {
		Buffer const* src;
		Buffer const* dst;
	};
	vstd::vector<vstd::vector<VkBufferCopy>> copyVecPool;
	vstd::HashMap<CopyBuffer, vstd::vector<VkBufferCopy>> copyCmds;

public:
	static constexpr size_t INIT_STACK_SIZE = 1024ull * 1024 * 4ull;
	FrameResource(Device const* device, CommandPool* pool);
	~FrameResource();
	FrameResource(FrameResource const&) = delete;
	FrameResource(FrameResource&&) = delete;
	vstd::optional<CommandBuffer> AllocateCmdBuffer();
	void Execute(FrameResource const* lastFrame);
	void InsertSemaphore(Event const* evt);
	void ExecuteCopy(CommandBuffer* cb);
	void Wait();
	void AddCopyCmd(
		Buffer const* src,
		uint64 srcOffset,
		Buffer const* dst,
		uint64 dstOffset,
		uint64 size);
	BufferView AllocateUpload(
		uint64 size,
		uint64 align = 0);
	BufferView AllocateReadback(
		uint64 size,
		uint64 align = 0);
	BufferView AllocateDefault(
		uint64 size,
		uint64 align = 0);
};
}// namespace toolhub::vk