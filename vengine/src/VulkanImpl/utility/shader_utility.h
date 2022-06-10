#pragma once
#include <vulkan_include.h>
#include <Common/functional.h>
namespace toolhub::vk {
class ShaderUtility {
	ShaderUtility(ShaderUtility const&) = delete;
	ShaderUtility(ShaderUtility&&) = delete;

public:
	static VkShaderModule LoadShader(vstd::span<vbyte const> data, VkDevice device);
};
}// namespace toolhub::vk