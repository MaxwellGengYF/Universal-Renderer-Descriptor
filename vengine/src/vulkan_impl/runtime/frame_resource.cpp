#include "frame_resource.h"
#include <vulkan_initializer.hpp>
#include "command_pool.h"
#include <gpu_collection/texture.h>
#include "res_state_tracker.h"
#include "event.h"
namespace toolhub::vk {
template<RWState state>
uint64 BufferStackVisitor<state>::Allocate(uint64 size) {
	return reinterpret_cast<uint64>(new Buffer(
		device,
		size,
		false,
		state,
		0));
}
template<RWState state>
void BufferStackVisitor<state>::DeAllocate(uint64 handle) {
	delete reinterpret_cast<Buffer*>(handle);
}
FrameResource::FrameResource(Device const* device, CommandPool* pool)
	: Resource(device), pool(pool),
	  descManager(device),
	  uploadAlloc(INIT_STACK_SIZE, &uploadVisitor),
	  defaultAlloc(INIT_STACK_SIZE, &defaultVisitor),
	  readBackAlloc(INIT_STACK_SIZE, &readbackVisitor) {
	uploadVisitor.device = device;
	readbackVisitor.device = device;
	defaultVisitor.device = device;
	VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	ThrowIfFailed(vkCreateFence(device->device, &fenceInfo, Device::Allocator(), &syncFence));
	vkResetFences(device->device, 1, &syncFence);
	VkSemaphoreCreateInfo semaphoreInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
	vkCreateSemaphore(
		device->device,
		&semaphoreInfo,
		Device::Allocator(),
		&semaphore);
}
FrameResource::~FrameResource() {
	vkDestroySemaphore(device->device, semaphore, Device::Allocator());
	vkDestroyFence(device->device, syncFence, Device::Allocator());
}
CommandBuffer* FrameResource::GetCmdBuffer() {
	if (cmdBuffer) return cmdBuffer;
	if (!vkCmdBuffer) {
		VkCommandBufferAllocateInfo allocInfo = vks::initializers::commandBufferAllocateInfo(
			pool->pool,
			VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			1);
		ThrowIfFailed(vkAllocateCommandBuffers(device->device, &allocInfo, &vkCmdBuffer));
	}
	cmdBuffer.New(&descManager, device, vkCmdBuffer, this);
	return cmdBuffer;
}
void FrameResource::Execute(FrameResource* lastFrame) {
	VkSubmitInfo computeSubmitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
	if (cmdBuffer) {
		cmdBuffer.Delete();
		computeSubmitInfo.commandBufferCount = 1;
		computeSubmitInfo.pCommandBuffers = &vkCmdBuffer;
	} else {
		computeSubmitInfo.commandBufferCount = 0;
		computeSubmitInfo.pCommandBuffers = nullptr;
	}
	VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	if (lastFrame && lastFrame->signaled) {
		lastFrame->signaled = false;
		computeSubmitInfo.waitSemaphoreCount = 1;
		computeSubmitInfo.pWaitSemaphores = &lastFrame->semaphore;
		computeSubmitInfo.pWaitDstStageMask = &waitStage;
	}
	if (!signaled) {
		signaled = true;
		computeSubmitInfo.signalSemaphoreCount = 1;
		computeSubmitInfo.pSignalSemaphores = &semaphore;
	}
	ThrowIfFailed(vkQueueSubmit(device->computeQueue, 1, &computeSubmitInfo, syncFence));
}
void FrameResource::SignalSemaphore(VkSemaphore semaphore) {
	VkSubmitInfo computeSubmitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
	computeSubmitInfo.signalSemaphoreCount = 1;
	computeSubmitInfo.pSignalSemaphores = &semaphore;
	ThrowIfFailed(vkQueueSubmit(device->computeQueue, 1, &computeSubmitInfo, syncFence));
}
void FrameResource::WaitSemaphore(VkSemaphore semaphore) {
	VkSubmitInfo computeSubmitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
	computeSubmitInfo.waitSemaphoreCount = 1;
	VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	computeSubmitInfo.pWaitSemaphores = &semaphore;
	computeSubmitInfo.pWaitDstStageMask = &waitStage;
	ThrowIfFailed(vkQueueSubmit(device->computeQueue, 1, &computeSubmitInfo, syncFence));
}

void FrameResource::ExecuteCopy() {
	CommandBuffer* cb = cmdBuffer;
	for (auto&& i : bufferCopyCmds) {
		auto&& vec = i.second;
		auto&& pair = i.first;
		vkCmdCopyBuffer(
			cb->cmdBuffer,
			pair.src->GetResource(),
			pair.dst->GetResource(),
			vec.size(),
			vec.data());
		vec.clear();
		bufferCopyVecPool.emplace_back(std::move(vec));
	}
	for (auto&& i : imgCopyCmds) {
		auto&& vec = i.second;
		auto&& pair = i.first;
		vkCmdCopyImage(
			cb->cmdBuffer,
			pair.src->GetResource(),
			VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
			pair.dst->GetResource(),
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			vec.size(),
			vec.data());
		vec.clear();
		imgCopyVecPool.emplace_back(std::move(vec));
	}
	for (auto&& i : bufImgCopyCmds) {
		auto&& vec = i.second;
		auto&& pair = i.first;
		vkCmdCopyBufferToImage(
			cb->cmdBuffer,
			pair.src->GetResource(),
			pair.dst->GetResource(),
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			vec.size(),
			vec.data());
		vec.clear();
		bufImgCopyVecPool.emplace_back(std::move(vec));
	}
	for (auto&& i : imgBufCopyCmds) {
		auto&& vec = i.second;
		auto&& pair = i.first;
		vkCmdCopyImageToBuffer(
			cb->cmdBuffer,
			pair.src->GetResource(),
			VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
			pair.dst->GetResource(),
			vec.size(),
			vec.data());
		vec.clear();
		bufImgCopyVecPool.emplace_back(std::move(vec));
	}
	bufferCopyCmds.Clear();
	imgCopyCmds.Clear();
	bufImgCopyCmds.Clear();
	imgBufCopyCmds.Clear();
}
void FrameResource::ExecuteScratchAlloc(ResStateTracker& stateTracker) {
	if (scratchAccumulateSize == 0) return;
	bfViewBegin = scratchBufferViews.begin();
	for (auto&& i : scratchBuffers) {
		if (i->ByteSize() >= scratchAccumulateSize) {
			curScratchBuffer = i.get();
			goto SKIP_NEW_SCRATCH_BUFFER;
		}
	}
	curScratchBuffer = new Buffer(
		device,
		scratchAccumulateSize,
		false,
		RWState::None,
		256);
	scratchBuffers.emplace_back(curScratchBuffer);
SKIP_NEW_SCRATCH_BUFFER:
	stateTracker.MarkBufferWrite(
		BufferView(curScratchBuffer, 0, scratchAccumulateSize),
		BufferWriteState::Accel);
	scratchAccumulateSize = 0;
}

void FrameResource::Wait() {
	ThrowIfFailed(vkWaitForFences(
		device->device,
		1,
		&syncFence,
		true,
		std::numeric_limits<uint64>::max()));
}
void FrameResource::Reset() {
	vkResetFences(device->device, 1, &syncFence);
	for (auto&& i : disposeFuncs) {
		i();
	}
	disposeFuncs.clear();
	uploadAlloc.Clear();
	defaultAlloc.Clear();
	readBackAlloc.Clear();
	descManager.EndFrame();
	scratchBuffers.clear();
}

namespace detail {
static BufferView AllocateBuffer(
	vstd::StackAllocator& alloc,
	uint64 size,
	uint64 align) {
	vstd::StackAllocator::Chunk b;
	if (align == 0)
		b = alloc.Allocate(size);
	else
		b = alloc.Allocate(size, align);
	return BufferView(reinterpret_cast<Buffer const*>(b.handle), b.offset, size);
}
}// namespace detail
BufferView FrameResource::AllocateUpload(
	uint64 size,
	uint64 align) {
	return detail::AllocateBuffer(uploadAlloc, size, align);
}
BufferView FrameResource::AllocateReadback(
	uint64 size,
	uint64 align) {
	return detail::AllocateBuffer(readBackAlloc, size, align);
}
BufferView FrameResource::AllocateDefault(
	uint64 size,
	uint64 align) {
	return detail::AllocateBuffer(defaultAlloc, size, align);
}
namespace detail {
template<typename Ele>
decltype(auto) GetVecPoolLazyEval(vstd::vector<Ele>& vec) {
	return vstd::MakeLazyEval([&]() -> Ele {
		if (vec.empty()) return {};
		return vec.erase_last();
	});
}
}// namespace detail
void FrameResource::AddCopyCmd(
	Buffer const* src,
	uint64 srcOffset,
	Buffer const* dst,
	uint64 dstOffset,
	uint64 size) {
	auto ite = bufferCopyCmds.Emplace(
		CopyKey<Buffer, Buffer>{src, dst},
		detail::GetVecPoolLazyEval(bufferCopyVecPool));
	ite.Value().emplace_back(VkBufferCopy{
		srcOffset,
		dstOffset,
		size});
}
void FrameResource::AddCopyCmd(
	Buffer const* src,
	Buffer const* dst,
	vstd::move_only_func<vstd::optional<VkBufferCopy>()> const& iterateFunc,
	size_t reserveSize) {
	auto ite = bufferCopyCmds.Emplace(
		CopyKey<Buffer, Buffer>{src, dst},
		detail::GetVecPoolLazyEval(bufferCopyVecPool));
	auto&& vec = ite.Value();
	vec.reserve(vec.size() + reserveSize);
	while (auto v = iterateFunc()) {
		vec.emplace_back(*v);
	}
}
void FrameResource::AddCopyCmd(
	Texture const* src,
	Texture const* dst,
	vstd::move_only_func<vstd::optional<VkImageCopy>()> const& iterateFunc,
	size_t reserveSize) {
	auto ite = imgCopyCmds.Emplace(
		CopyKey<Texture, Texture>{src, dst},
		detail::GetVecPoolLazyEval(imgCopyVecPool));
	auto&& vec = ite.Value();
	vec.reserve(vec.size() + reserveSize);
	while (auto v = iterateFunc()) {
		vec.emplace_back(*v);
	}
}
void FrameResource::AddCopyCmd(
	Buffer const* src,
	Texture const* dst,
	vstd::move_only_func<vstd::optional<VkBufferImageCopy>()> const& iterateFunc,
	size_t reserveSize) {
	auto ite = bufImgCopyCmds.Emplace(
		CopyKey<Buffer, Texture>{src, dst},
		detail::GetVecPoolLazyEval(bufImgCopyVecPool));
	auto&& vec = ite.Value();
	vec.reserve(vec.size() + reserveSize);
	while (auto v = iterateFunc()) {
		vec.emplace_back(*v);
	}
}
void FrameResource::AddCopyCmd(
	Texture const* src,
	Buffer const* dst,
	vstd::move_only_func<vstd::optional<VkBufferImageCopy>()> const& iterateFunc,
	size_t reserveSize) {
	auto ite = imgBufCopyCmds.Emplace(
		CopyKey<Texture, Buffer>{src, dst},
		detail::GetVecPoolLazyEval(bufImgCopyVecPool));
	auto&& vec = ite.Value();
	vec.reserve(vec.size() + reserveSize);
	while (auto v = iterateFunc()) {
		vec.emplace_back(*v);
	}
}
void FrameResource::AddCopyCmd(
	Texture const* src,
	uint srcMip,
	Texture const* dst,
	uint dstMip) {
	auto ite = imgCopyCmds.Emplace(
		CopyKey<Texture, Texture>{src, dst},
		detail::GetVecPoolLazyEval(imgCopyVecPool));
	auto&& v = ite.Value().emplace_back();
	v.srcSubresource.aspectMask = 0;
	v.srcSubresource.baseArrayLayer = 0;
	v.srcSubresource.layerCount = 1;
	v.srcSubresource.mipLevel = srcMip;
	v.dstSubresource.aspectMask = 0;
	v.dstSubresource.baseArrayLayer = 0;
	v.dstSubresource.layerCount = 1;
	v.dstSubresource.mipLevel = dstMip;
	auto sz = src->Size();
	v.extent = {sz.x, sz.y, sz.z};
	v.srcOffset = {0, 0, 0};
	v.dstOffset = {0, 0, 0};
}
void FrameResource::AddCopyCmd(
	Buffer const* src,
	uint64 srcOffset,
	Texture const* dst,
	uint dstMip) {
	auto ite = bufImgCopyCmds.Emplace(
		CopyKey<Buffer, Texture>{src, dst},
		detail::GetVecPoolLazyEval(bufImgCopyVecPool));
	auto&& v = ite.Value().emplace_back();
	v.bufferOffset = srcOffset;
	v.bufferRowLength = 0;
	v.bufferImageHeight = 0;
	v.imageSubresource.aspectMask = 0;
	v.imageSubresource.baseArrayLayer = 0;
	v.imageSubresource.layerCount = 1;
	v.imageSubresource.mipLevel = dstMip;
	v.imageOffset = {0, 0, 0};
	auto sz = dst->Size();
	v.imageExtent = {sz.x, sz.y, sz.z};
}
void FrameResource::AddCopyCmd(
	Texture const* src,
	uint srcMipOffset,
	uint srcMip,
	Buffer const* dst,
	uint64 dstOffset) {
	auto ite = imgBufCopyCmds.Emplace(
		CopyKey<Texture, Buffer>{src, dst},
		detail::GetVecPoolLazyEval(bufImgCopyVecPool));
	auto&& v = ite.Value().emplace_back();
	v.bufferOffset = dstOffset;
	v.bufferRowLength = 0;
	v.bufferImageHeight = 0;
	v.imageSubresource.aspectMask = 0;
	v.imageSubresource.baseArrayLayer = 0;
	v.imageSubresource.layerCount = 1;
	v.imageSubresource.mipLevel = srcMip;
	v.imageOffset = {0, 0, 0};
	auto sz = src->Size();
	v.imageExtent = {sz.x, sz.y, sz.z};
}
void FrameResource::AddDisposeEvent(vstd::move_only_func<void()>&& disposeFunc) {
	disposeFuncs.emplace_back(std::move(disposeFunc));
}
void FrameResource::ResetScratch() {
	scratchBufferViews.clear();
	scratchAccumulateSize = 0;
}
void FrameResource::AddScratchSize(size_t size) {
	scratchAccumulateSize += size;
	scratchBufferViews.emplace_back(size);
}
BufferView FrameResource::GetScratchBufferView() {
	BufferView bf(
		curScratchBuffer,
		scratchAccumulateSize,
		*bfViewBegin);
	scratchAccumulateSize += *bfViewBegin;
	bfViewBegin++;
	return bf;
}
}// namespace toolhub::vk