#pragma once
#include <vulkan_include.h>
namespace toolhub::vk {
class Buffer;
struct BufferView {
	Buffer const* buffer;
	size_t offset;
	size_t size;
	BufferView() : buffer(nullptr), offset(0), size(0){}
	BufferView(
		Buffer const* buffer,
		size_t offset,
		size_t size)
		: buffer(buffer),
		  offset(offset),
		  size(size) {}
	BufferView(
		Buffer const* buffer,
		size_t offset);
	BufferView(
		Buffer const* buffer);
};
}// namespace toolhub::vk