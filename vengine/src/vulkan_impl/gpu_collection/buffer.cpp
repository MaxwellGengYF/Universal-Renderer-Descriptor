#include <gpu_collection/buffer.h>
#include <types/buffer_view.h>
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
				  case RWState::Accel:
					  return (VkBufferUsageFlagBits)(VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
				  default:
					  return (VkBufferUsageFlagBits)(VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
			  }
		  }(rwState),
		  crossQueue,
		  rwState,
		  align) {
}
void Buffer::CopyFrom(vstd::span<vbyte const> data, size_t offset) const {
	memcpy(reinterpret_cast<vbyte*>(vmaBuffer.mappedPtr) + offset, data.data(), data.size());
}
void Buffer::CopyTo(vstd::span<vbyte> data, size_t offset) const {
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
VkDeviceAddress Buffer::GetAddress(size_t offset) const {
	VkBufferDeviceAddressInfo addressCreateInfo{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
	addressCreateInfo.buffer = vmaBuffer.buffer;
	return vkGetBufferDeviceAddress(
			   device->device,
			   &addressCreateInfo)
		   + offset;
}

}// namespace toolhub::vk