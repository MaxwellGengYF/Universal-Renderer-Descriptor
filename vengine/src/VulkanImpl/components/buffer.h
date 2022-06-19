#pragma once
#include <components/gpu_collection.h>
#include <components/gpu_allocator/vma_buffer.h>
namespace toolhub::vk {
class Buffer : public GPUCollection {
	VmaBuffer vmaBuffer;
	size_t byteSize;

public:
	VkBuffer GetResource() const { return vmaBuffer.buffer; }
	size_t ByteSize() const { return byteSize; }
	Buffer(Buffer const&) = delete;
	Buffer(Buffer&&) = delete;
	Buffer(
		Device const* device,
		size_t byteSize,
		bool crossQueue,
		RWState rwState,
		size_t alignment = 0);
	void CopyTo(vstd::span<vbyte> data, size_t offset);
	void CopyFrom(vstd::span<vbyte const> data, size_t offset);
	VkDescriptorBufferInfo GetDescriptor(size_t offset = 0, size_t size = VK_WHOLE_SIZE) const;
	~Buffer();
	Tag GetTag() const override{return Tag::Buffer;}

};
}// namespace toolhub::vk