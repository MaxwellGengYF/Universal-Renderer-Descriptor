#pragma once
#include <components/resource.h>
#include "command_buffer.h"
#include <Utility/StackAllocator.h>
#include <gpu_collection/buffer.h>
#include <shader/descriptorset_manager.h>
#include <Common/functional.h>
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
	VkCommandBuffer vkCmdBuffer = nullptr;
	vstd::optional<CommandBuffer> cmdBuffer;
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
	template<typename Src, typename Dst>
	struct CopyKey {
		Src const* src;
		Dst const* dst;
	};
	// Copy
	vstd::vector<vstd::vector<VkBufferCopy>> bufferCopyVecPool;
	vstd::vector<vstd::vector<VkImageCopy>> imgCopyVecPool;
	vstd::vector<vstd::vector<VkBufferImageCopy>> bufImgCopyVecPool;
	template<typename Src, typename Dst, typename Value>
	using Map = vstd::HashMap<CopyKey<Src, Dst>, vstd::vector<Value>>;
	Map<Buffer, Buffer, VkBufferCopy> bufferCopyCmds;
	Map<Texture, Texture, VkImageCopy> imgCopyCmds;
	Map<Buffer, Texture, VkBufferImageCopy> bufImgCopyCmds;
	Map<Texture, Buffer, VkBufferImageCopy> imgBufCopyCmds;
	// Sync
	vstd::vector<vstd::move_only_func<void()>> disposeFuncs;
	vstd::vector<vstd::unique_ptr<Buffer>> scratchBuffers;
	vstd::vector<size_t> scratchBufferViews;
	size_t* bfViewBegin = nullptr;
	Buffer* curScratchBuffer = nullptr;
	size_t scratchAccumulateSize = 0;
	bool signaled = false;

public:
	static constexpr size_t INIT_STACK_SIZE = 1024ull * 1024 * 4ull;
	FrameResource(Device const* device, CommandPool* pool);
	~FrameResource();
	FrameResource(FrameResource const&) = delete;
	FrameResource(FrameResource&&) = delete;
	CommandBuffer* GetCmdBuffer();
	void Execute(FrameResource* lastFrame);
	void InsertSemaphore(Event const* evt);
	void ExecuteCopy();
	void ExecuteScratchAlloc(ResStateTracker& stateTracker);
	void Wait();
	void Reset();
	void AddDisposeEvent(vstd::move_only_func<void()>&& disposeFunc);
	void AddCopyCmd(
		Buffer const* src,
		uint64 srcOffset,
		Buffer const* dst,
		uint64 dstOffset,
		uint64 size);
	void AddCopyCmd(
		Buffer const* src,
		Buffer const* dst,
		vstd::move_only_func<vstd::optional<VkBufferCopy>()> const& iterateFunc,
		size_t reserveSize = 0);
	void AddCopyCmd(
		Texture const* src,
		uint srcMip,
		Texture const* dst,
		uint dstMip);
	void AddCopyCmd(
		Texture const* src,
		Texture const* dst,
		vstd::move_only_func<vstd::optional<VkImageCopy>()> const& iterateFunc,
		size_t reserveSize = 0);
	void AddCopyCmd(
		Buffer const* src,
		uint64 srcOffset,
		Texture const* dst,
		uint dstMipOffset);
	void AddCopyCmd(
		Buffer const* src,
		Texture const* dst,
		vstd::move_only_func<vstd::optional<VkBufferImageCopy>()> const& iterateFunc,
		size_t reserveSize = 0);
	void AddCopyCmd(
		Texture const* src,
		uint srcMipOffset,
		uint srcMipCount,
		Buffer const* dst,
		uint64 dstOffset);
	void AddCopyCmd(
		Texture const* src,
		Buffer const* dst,
		vstd::move_only_func<vstd::optional<VkBufferImageCopy>()> const& iterateFunc,
		size_t reserveSize = 0);

	BufferView AllocateUpload(
		uint64 size,
		uint64 align = 0);
	BufferView AllocateReadback(
		uint64 size,
		uint64 align = 0);
	BufferView AllocateDefault(
		uint64 size,
		uint64 align = 0);
	void SignalSemaphore(VkSemaphore semaphore);
	void WaitSemaphore(VkSemaphore semaphore);
	// Accels
	void ResetScratch();
	void AddScratchSize(size_t size);
	BufferView GetScratchBufferView();
};
}// namespace toolhub::vk