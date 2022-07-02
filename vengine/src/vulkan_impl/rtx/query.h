#pragma once
#include <components/resource.h>
namespace toolhub::vk {
class Query : public Resource {
	VkQueryPool queryPool = nullptr;
	size_t mCount = 0;
	size_t mCapacity = 0;

public:
	VkQueryPool Pool() const { return queryPool; }
	Query(Device const* device, size_t initSize = 1024);
	~Query();
	void Reset(size_t count);
	void Readback(
		vstd::span<uint64> result) const;
};
}// namespace toolhub::vk