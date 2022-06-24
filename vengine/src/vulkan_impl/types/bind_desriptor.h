#pragma once
#include <vulkan_impl/vulkan_include.h>
#include <vulkan_impl/types/buffer_view.h>
#include <vulkan_impl/types/tex_view.h>
namespace toolhub::vk {
class Texture;
class Buffer;
class Accel;
struct BindResource {
	vstd::variant<
		Texture const*,
		Buffer const*,
		Accel const*>
		res;
	size_t offset;
	size_t size;
	bool writable;
	BindResource(BufferView const& bf, bool writable)
		: res(bf.buffer),
		  offset(bf.offset),
		  size(bf.size),
		  writable(writable) {}
	BindResource(TexView const& tex, bool writable)
		: res(tex.tex),
		  offset(tex.mipOffset),
		  writable(writable),
		  size(tex.mipCount) {}
	BindResource(Accel const* accel)
		: res(accel) {}
};
}// namespace toolhub::vk