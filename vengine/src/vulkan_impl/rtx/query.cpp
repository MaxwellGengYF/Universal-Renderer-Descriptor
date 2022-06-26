#include "Query.h"
namespace toolhub::vk {
Query::Query(Device const* device, size_t initSize)
	: Resource(device), mCapacity(initSize) {
	VkQueryPoolCreateInfo createInfo{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
	createInfo.queryType = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR;
	createInfo.queryCount = mCapacity;
	ThrowIfFailed(vkCreateQueryPool(
		device->device,
		&createInfo,
		Device::Allocator(),
		&queryPool));
}
Query::~Query() {
	if (queryPool) {
		vkDestroyQueryPool(
			device->device,
			queryPool,
			Device::Allocator());
	}
}
void Query::Reset(size_t count) {
	if (mCapacity < count) {
		do {
			mCapacity = mCapacity * 1.5 + 8;
		} while (mCapacity < count);
		if (queryPool) {
			vkDestroyQueryPool(
				device->device,
				queryPool,
				Device::Allocator());
		}
		VkQueryPoolCreateInfo createInfo{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
		createInfo.queryType = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR;
		createInfo.queryCount = mCapacity;
		ThrowIfFailed(vkCreateQueryPool(
			device->device,
			&createInfo,
			Device::Allocator(),
			&queryPool));
	}
	mCount = count;
	vkResetQueryPool(
		device->device,
		queryPool,
		0,
		count);
}
void Query::Readback(
	vstd::span<uint64> result) const {
	ThrowIfFailed(vkGetQueryPoolResults(
		device->device, queryPool, 0, mCount,
		result.size_bytes(),
		result.data(),
		sizeof(uint64),
		VK_QUERY_RESULT_64_BIT));
}

}// namespace toolhub::vk