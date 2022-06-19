#pragma once
#include <vulkan_include.h>
#include <types/buffer_view.h>
#include <types/tex_view.h>
namespace toolhub::vk {
class Texture;
class Buffer;

using BindDescriptor = vstd::variant<
	VkDescriptorBufferInfo,
	VkDescriptorImageInfo>;
struct BindResource {
	using Variant =
		vstd::variant<
			Texture const*,
			Buffer const*>;
	Variant res;
	size_t offset;
	size_t size;
	bool writable;
	template<typename T>
	requires(std::is_constructible_v<Variant, T>)
		BindResource(
			T resPtr,
			size_t offset,
			size_t size,
			bool writable)
		: res(resPtr),
		  offset(offset),
		  size(size),
		  writable(writable) {}
	BindDescriptor GetDescriptor() const;
	BindResource(
		BufferView const& bfView,
		bool write)
		: BindResource(bfView.buffer, bfView.offset, bfView.size, write) {}
	BindResource(
		TexView const& texView,
		bool write)
		: BindResource(texView.tex, texView.mipOffset, texView.mipCount, write) {}
};

}// namespace toolhub::vk