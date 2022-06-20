#include <components/bindless_array.h>
namespace toolhub::vk {
BindlessArray::BindlessArray(
	Device const* device,
	uint arrSize)
	: GPUCollection(device) {
}
BindlessArray::~BindlessArray() {
}
void BindlessArray::AddRef(GPUCollection const* v) {
    refMap.Emplace(v, 0).Value()++;
}
void BindlessArray::RemoveRef(GPUCollection const* v) {
    auto ite = refMap.Find(v);
    if(!ite) return;
    if(--ite.Value() == 0){
        refMap.Remove(ite);
    }
}
bool BindlessArray::IsPtrInRes(GPUCollection const* handle) const {
    return refMap.Find(handle);
}

}// namespace toolhub::vk