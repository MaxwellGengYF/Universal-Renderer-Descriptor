#pragma once
#include <vulkan_include.h>
namespace toolhub::vk {
using BindDescriptor = vstd::variant<
	VkDescriptorBufferInfo,
	VkDescriptorImageInfo>;
}