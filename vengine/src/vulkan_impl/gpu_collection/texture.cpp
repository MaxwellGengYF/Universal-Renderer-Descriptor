#include <vulkan_impl/gpu_collection/texture.h>
#include <vulkan_impl/types/tex_view.h>
namespace toolhub::vk {
Texture::Texture(
	Device const* device,
	uint3 size,
	VkFormat format,
	VkImageType dimension,
	uint mip)
	: GPUCollection(device),
	  image(
		  *device->gpuAllocator,
		  dimension, size,
		  mip, 1, format),
	  size(size),
	  format(format),
	  dimension(dimension),
	  mip(mip) {
	imageLayouts = vstd::create_unique(vengine_new_array<std::atomic_uint, uint>(mip, VK_IMAGE_LAYOUT_PREINITIALIZED));
}
VkImageLayout Texture::TransformLayout(VkImageLayout newLayout, uint mipLevel) const {
	return VkImageLayout(imageLayouts.get()[mipLevel].exchange(newLayout));
}
VkImageLayout Texture::GetLayout(uint mipLevel) const {
	return static_cast<VkImageLayout>(imageLayouts.get()[mipLevel].load());
}

Texture::~Texture() {
	for (auto&& i : imgViews) {
		vkDestroyImageView(
			device->device,
			i.second,
			Device::Allocator());
	}
}
TexView::TexView(
	Texture const* tex,
	uint mipOffset,
	uint mipCount)
	: tex(tex),
	  mipOffset(mipOffset),
	  mipCount(mipCount) {
}
TexView::TexView(
	Texture const* tex,
	uint mipOffset)
	: tex(tex),
	  mipOffset(mipOffset),
	  mipCount(tex->Mip() - mipOffset) {
}
TexView::TexView(
	Texture const* tex)
	: mipOffset(0),
	  tex(tex),
	  mipCount(tex->Mip()) {
}
VkImageView Texture::GetImageView(uint mipLayer, uint mipCount) const {
	VkImageViewCreateInfo createInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
	createInfo.image = image.image;
	switch (dimension) {
		case VK_IMAGE_TYPE_1D:
			createInfo.viewType = VK_IMAGE_VIEW_TYPE_1D;
			break;
		case VK_IMAGE_TYPE_2D:
			createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			break;
		case VK_IMAGE_TYPE_3D:
			createInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
			break;
	}
	createInfo.format = format;
	createInfo.components = {
		VK_COMPONENT_SWIZZLE_R,
		VK_COMPONENT_SWIZZLE_G,
		VK_COMPONENT_SWIZZLE_B,
		VK_COMPONENT_SWIZZLE_A,
	};
	auto&& subRange = createInfo.subresourceRange;
	subRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subRange.baseMipLevel = mipLayer;
	subRange.levelCount = mipCount;
	subRange.baseArrayLayer = 0;
	subRange.layerCount = 1;

	std::lock_guard lck(mtx);
	return imgViews
		.Emplace(
			std::pair<uint, uint>(mipLayer, mipCount),
			vstd::MakeLazyEval([&]() {
				VkImageView imgView;
				vkCreateImageView(
					device->device,
					&createInfo,
					Device::Allocator(),
					&imgView);
				return imgView;
			}))
		.Value();
}

VkDescriptorImageInfo Texture::GetDescriptor(uint mip, uint mipCount) const {
	VkDescriptorImageInfo info;
	info.imageView = GetImageView(mip, mipCount);
	info.imageLayout = VkImageLayout(imageLayouts.get()[mip].load());
	return info;
}
}// namespace toolhub::vk