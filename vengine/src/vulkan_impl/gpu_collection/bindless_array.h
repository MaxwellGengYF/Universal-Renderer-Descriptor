#pragma once
#include <vulkan_impl/gpu_collection/gpu_collection.h>
namespace toolhub::vk {
class GPUCollection;
class BindlessArray : public GPUCollection {
	vstd::HashMap<GPUCollection const*, size_t> refMap;
	void AddRef(GPUCollection const* v);
	void RemoveRef(GPUCollection const* v);

public:
	bool IsPtrInRes(GPUCollection const* handle) const;
	Tag GetTag() const {
		return Tag::BindlessArray;
	}
	BindlessArray(
		Device const* device,
		uint arrSize);
	~BindlessArray();
};
}// namespace toolhub::vk