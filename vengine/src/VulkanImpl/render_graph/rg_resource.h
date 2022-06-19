#pragma once
#include <vulkan_include.h>
#include <Common/small_vector.h>
#include <Common/functional.h>
namespace toolhub::vk {
class RGResource;
class RGResourceCollection;
using RGResCollectMap = vstd::HashMap<RGResource*, size_t>;
using RGResMap = vstd::HashMap<RGResourceCollection*, RGResCollectMap::Index>;
class RGResourceCollection final : public vstd::IOperatorNewBase {
	friend class RGResource;

protected:
	RGResCollectMap map;
	vstd::spin_mutex mtx;

public:
	RGResourceCollection(RGResourceCollection const&) = delete;
	RGResourceCollection(RGResourceCollection&&) = delete;
	void BindResource(RGResource* res);
	void UnBindResource(RGResource* res);
	~RGResourceCollection();
};

class RGResource final : public vstd::IOperatorNewBase {
	friend class RGResourceCollection;

protected:
	RGResMap collectMap;
	mutable vstd::spin_mutex resMtx;

private:
	bool AddSelfToMap(RGResourceCollection* dstMap, RGResCollectMap::Index index);
	void ClearSelfInMap(RGResourceCollection* dstMap);
	RGResource(RGResource const&) = delete;
	RGResource(RGResource&& v) = delete;

public:
	RGResource();
	~RGResource();
	void ThreadSafeIterate(vstd::move_only_func<void(RGResourceCollection&)> const& func) const;
};

}// namespace toolhub::vk BufferView 
