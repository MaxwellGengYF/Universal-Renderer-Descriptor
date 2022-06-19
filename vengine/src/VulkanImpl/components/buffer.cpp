#include <components/buffer.h>
#include <components/buffer_view.h>
namespace toolhub::vk {
Buffer::Buffer(
	Device const* device,
	size_t byteSize,
	bool crossQueue,
	RWState rwState,
	size_t align)
	: GPUCollection(device),
	  byteSize(byteSize),
	  vmaBuffer(
		  *device->gpuAllocator,
		  byteSize,
		  [](RWState state) {
			  switch (state) {
				  case RWState::Upload:
					  return VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
				  case RWState::Readback:
					  return VK_BUFFER_USAGE_TRANSFER_DST_BIT;
				  default:
					  return (VkBufferUsageFlagBits)((uint)VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | (uint)VK_BUFFER_USAGE_TRANSFER_DST_BIT | (uint)VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
			  }
		  }(rwState),
		  crossQueue,
		  rwState,
		  align) {
}
void Buffer::CopyFrom(vstd::span<vbyte const> data, size_t offset) {
	memcpy(reinterpret_cast<vbyte*>(vmaBuffer.mappedPtr) + offset, data.data(), data.size());
}
void Buffer::CopyTo(vstd::span<vbyte> data, size_t offset) {
	memcpy(data.data(), reinterpret_cast<vbyte const*>(vmaBuffer.mappedPtr) + offset, data.size());
}
VkDescriptorBufferInfo Buffer::GetDescriptor(
	size_t offset, size_t size) const {
	VkDescriptorBufferInfo bufferInfo{
		vmaBuffer.buffer,
		offset,
		size};
	return bufferInfo;
}
BufferView::BufferView(
	Buffer const* buffer,
	size_t offset)
	: buffer(buffer),
	  offset(offset),
	  size(buffer->ByteSize() - offset) {}
BufferView::BufferView(
	Buffer const* buffer)
	: buffer(buffer),
	  offset(0),
	  size(buffer->ByteSize()) {}

Buffer::~Buffer() {}
}// namespace toolhub::vk