#include <render_graph/rg_resource.h>
namespace toolhub::vk {
void RGResource::ClearSelfInMap(RGResourceCollection* dstMap) {
	std::lock_guard lck(resMtx);
	auto ite = collectMap.Find(dstMap);
	if (!ite) return;
	collectMap.Remove(ite);
}
bool RGResource::AddSelfToMap(RGResourceCollection* dstMap, RGResCollectMap::Index index) {
	std::lock_guard lck(resMtx);
	return collectMap.TryEmplace(dstMap, index).second;
}

RGResource::~RGResource() {
	for (auto&& i : collectMap) {
		std::lock_guard lck(i.first->mtx);
		i.first->map.Remove(i.second);
	}
}
void RGResourceCollection::BindResource(RGResource* res) {
	std::unique_lock lck(mtx);
	auto ite = map.TryEmplace(res, 1);
	if (ite.second) {
		lck.unlock();
		res->AddSelfToMap(this, ite.first);
	} else {
		ite.first.Value()++;
	}
}
void RGResourceCollection::UnBindResource(RGResource* res) {
	size_t refCount;
	{
		std::lock_guard lck(mtx);
		auto ite = map.Find(res);
		if (!ite) return;
		refCount = --ite.Value();
	}
	if (refCount == 0) {
		res->ClearSelfInMap(this);
	}
}
RGResourceCollection::~RGResourceCollection() {
	for (auto&& i : map) {
		i.first->ClearSelfInMap(this);
	}
}
RGResource::RGResource(){}
void RGResource::ThreadSafeIterate(vstd::move_only_func<void(RGResourceCollection&)> const& func) const{
	std::lock_guard lck(resMtx);
	for(auto&& i : collectMap){
		func(*i.first);
	}
}

}// namespace toolhub::vk