#pragma once
#include <gpu_collection/gpu_collection.h>
#include <gpu_allocator/vma_image.h>
namespace toolhub::vk {
class Texture : public GPUCollection {
	VmaImage image;
	uint3 size;
	VkFormat format;
	VkImageType dimension;
	uint mip;
	vstd::unique_ptr<std::atomic_uint> imageLayouts;
	mutable vstd::HashMap<std::pair<uint, uint>, VkImageView> imgViews;
	mutable vstd::spin_mutex mtx;

public:
	VkImageLayout GetLayout(uint mipLevel) const;
	VkImageLayout TransformLayout(VkImageLayout newLayout, uint mipLevel) const;
	VkImage GetResource() const { return image.image; }
	Texture(
		Device const* device,
		uint3 size,
		VkFormat format,
		VkImageType dimension,
		uint mip);
	~Texture();
	VkImageView GetImageView(uint mipLayer, uint mipCount) const;
	uint3 Size() const { return size; }
	VkFormat Format() const { return format; }
	VkImageType Dimension() const { return dimension; }
	uint Mip() const { return mip; }
	Tag GetTag() const override { return Tag::Texture; }
	VkDescriptorImageInfo GetDescriptor(uint mipOffset, uint mipCount) const;
};
}// namespace toolhub::vk