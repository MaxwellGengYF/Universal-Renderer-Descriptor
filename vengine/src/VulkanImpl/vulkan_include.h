#pragma once
#include <vulkan/vulkan.h>
#include <Common/Common.h>
#include <glm/Include.h>
using namespace glm;
namespace toolhub::vk {
static constexpr uint32_t VulkanApiVersion = VK_API_VERSION_1_3;
//0: texture
//1: buffer
//2: sampler
inline void ThrowIfFailed(VkResult result) {
#ifdef DEBUG
	if (result != VK_SUCCESS) {
		VEngine_Log("Vulkan Failed!");
		VENGINE_EXIT;
	}
#endif
}
}// namespace toolhub::vk