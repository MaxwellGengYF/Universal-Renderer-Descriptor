#include <components/default_buffer.h>
namespace toolhub::vk {
DefaultBuffer::DefaultBuffer(
	Device const* device,
	size_t byteSize,
	VkBufferUsageFlagBits flag,
	bool crossQueue)
	: Resource(device),
	  byteSize(byteSize),
	  vmaBuffer(
		  *device->gpuAllocator,
		  byteSize,
		  flag,
		  crossQueue,
		  VmaBuffer::RWState::None) {
}
DefaultBuffer::~DefaultBuffer() {}
}// namespace toolhub::vk