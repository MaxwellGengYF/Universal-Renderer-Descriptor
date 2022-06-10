#pragma once
#include <vulkan/vulkan.h>
#include <Common/Common.h>
namespace toolhub::vk {
inline void ThrowIfFailed(VkResult result) {
#ifdef DEBUG
	if (result != VK_SUCCESS) {
		VEngine_Log("Vulkan Failed!");
		VENGINE_EXIT;
	}
#endif
}
}// namespace toolhub::vk