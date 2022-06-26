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
	template<typename Src, typename Dst>
	struct CopyKey {
		Src const* src;
		Dst const* dst;
	};
	vstd::vector<vstd::vector<VkBufferCopy>> bufferCopyVecPool;
	vstd::vector<vstd::vector<VkImageCopy>> imgCopyVecPool;
	vstd::vector<vstd::vector<VkBufferImageCopy>> bufImgCopyVecPool;
	template<typename Src, typename Dst, typename Value>
	using Map = vstd::HashMap<CopyKey<Src, Dst>, vstd::vector<Value>>;
	Map<Buffer, Buffer, VkBufferCopy> bufferCopyCmds;
	Map<Texture, Texture, VkImageCopy> imgCopyCmds;
	Map<Buffer, Texture, VkBufferImageCopy> bufImgCopyCmds;
	Map<Texture, Buffer, VkBufferImageCopy> imgBufCopyCmds;

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
	void AddCopyCmd(
		Texture const* src,
		uint srcMip,
		Texture const* dst,
		uint dstMip);
	void AddCopyCmd(
		Buffer const* src,
		uint64 srcOffset,
		Texture const* dst,
		uint dstMipOffset);
	void AddCopyCmd(
		Texture const* src,
		uint srcMipOffset,
		uint srcMipCount,
		Buffer const* dst,
		uint64 dstOffset);

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