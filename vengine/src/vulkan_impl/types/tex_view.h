#pragma once
#include <vulkan_include.h>
namespace toolhub::vk {
class Texture;
struct TexView {
	Texture const* tex;
	uint mipOffset;
	uint mipCount;
	TexView(
		Texture const* tex,
		uint mipOffset,
		uint mipCount);
	TexView(
		Texture const* tex,
		uint mipOffset);
	TexView(
		Texture const* tex);
};
}// namespace toolhub::vk