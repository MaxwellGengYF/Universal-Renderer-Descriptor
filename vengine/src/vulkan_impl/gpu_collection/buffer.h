#pragma once
#include <gpu_collection/gpu_collection.h>
#include <gpu_allocator/vma_buffer.h>
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
	void CopyTo(vstd::span<vbyte> data, size_t offset) const;
	void CopyFrom(vstd::span<vbyte const> data, size_t offset) const;
	template<typename T>
	void CopyValueTo(T& data, size_t offset) const {
		CopyTo(
			{reinterpret_cast<vbyte*>(&data), sizeof(T)},
			offset);
	}
	template<typename T>
	void CopyArrayTo(T* data, size_t count, size_t offset) const {
		CopyTo(
			{reinterpret_cast<vbyte*>(data), sizeof(T) * count},
			offset);
	}
	template<typename T>
	void CopyValueFrom(T const& data, size_t offset) const {
		CopyFrom(
			{reinterpret_cast<vbyte const*>(&data), sizeof(T)},
			offset);
	}
	template<typename T>
	void CopyArrayFrom(T const* data, size_t count, size_t offset) const {
		CopyFrom(
			{reinterpret_cast<vbyte const*>(data), sizeof(T) * count},
			offset);
	}
	VkDescriptorBufferInfo GetDescriptor(size_t offset = 0, size_t size = VK_WHOLE_SIZE) const;
	~Buffer();
	Tag GetTag() const override { return Tag::Buffer; }
	VkDeviceAddress GetAddress(size_t offset) const;
};
}// namespace toolhub::vk