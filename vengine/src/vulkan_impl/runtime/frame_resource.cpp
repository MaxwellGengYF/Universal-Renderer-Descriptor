#include "frame_resource.h"
#include <vulkan_impl/vulkan_initializer.hpp>
#include "command_pool.h"
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
void FrameResource::ReleaseCmdBuffer(VkCommandBuffer buffer) {
	allocatedBuffers.emplace_back(buffer);
}
vstd::optional<CommandBuffer> FrameResource::AllocateCmdBuffer() {
	if (executing) return {};
	VkCommandBuffer vkCmdBuffer;
	if (allocatedBuffers.empty()) {
		VkCommandBufferAllocateInfo allocInfo = vks::initializers::commandBufferAllocateInfo(
			pool->pool,
			VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			1);
		ThrowIfFailed(vkAllocateCommandBuffers(device->device, &allocInfo, &vkCmdBuffer));
	} else {
		vkCmdBuffer = allocatedBuffers.erase_last();
	}
	needExecuteBuffers.emplace_back(vkCmdBuffer);
	return CommandBuffer(device, vkCmdBuffer, this);
}
void FrameResource::Execute(FrameResource const* lastFrame) {
	if (executing && !needExecuteBuffers.empty()) return;
	VkSubmitInfo computeSubmitInfo = vks::initializers::submitInfo();
	computeSubmitInfo.commandBufferCount = needExecuteBuffers.size();
	computeSubmitInfo.pCommandBuffers = needExecuteBuffers.data();
	if (lastFrame) {
		computeSubmitInfo.waitSemaphoreCount = 1;
		VkSemaphore phore = lastFrame->semaphore;
		computeSubmitInfo.pWaitSemaphores = &phore;
	}
	/*
	computeSubmitInfo.waitSemaphoreCount = 1;
	computeSubmitInfo.pWaitSemaphores = &graphics.semaphore;
	computeSubmitInfo.pWaitDstStageMask = &waitStageMask;
	computeSubmitInfo.signalSemaphoreCount = 1;
	computeSubmitInfo.pSignalSemaphores = &compute.semaphore;*/
	ThrowIfFailed(vkQueueSubmit(device->computeQueue, 1, &computeSubmitInfo, syncFence));
	executing = true;
	needExecuteBuffers.clear();
}
void FrameResource::ExecuteCopy(CommandBuffer* cb) {
	for (auto&& i : copyCmds) {
		auto&& vec = i.second;
		vkCmdCopyBuffer(
			cb->cmdBuffer,
			i.first.src->GetResource(),
			i.first.dst->GetResource(),
			vec.size(),
			vec.data());
		vec.clear();
		copyVecPool.emplace_back(std::move(vec));
	}
	copyCmds.Clear();
}
void FrameResource::AddCopyCmd(
	Buffer const* src,
	uint64 srcOffset,
	Buffer const* dst,
	uint64 dstOffset,
	uint64 size) {
	auto ite = copyCmds.Emplace(
		CopyBuffer{src, dst},
		vstd::MakeLazyEval([&]() -> vstd::vector<VkBufferCopy> {
			if (copyVecPool.empty()) return {};
			return copyVecPool.erase_last();
		}));
	ite.Value().emplace_back(VkBufferCopy{
		srcOffset,
		dstOffset,
		size});
}
void FrameResource::Wait() {
	ThrowIfFailed(vkWaitForFences(
		device->device,
		1,
		&syncFence,
		true,
		std::numeric_limits<uint64>::max()));
	executing = false;
	vkResetFences(device->device, 1, &syncFence);
	uploadAlloc.Clear();
	defaultAlloc.Clear();
	readBackAlloc.Clear();
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
}// namespace toolhub::vk