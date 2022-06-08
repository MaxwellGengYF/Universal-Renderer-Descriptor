#include <ir/serialize/deser_context.h>
namespace toolhub::ir {
void DeserContext::InitDeser(
	vstd::span<vbyte const> data,
	vstd::ObjectStackAlloc* alloc) {
	objs.clear();
	this->alloc = alloc;
	this->data = data;
	uint64 size = *reinterpret_cast<uint64 const*>(data.data());
	uint64 objCount = (data.size() - size) / sizeof(uint64);
	uint64 const* objIndexPtr = reinterpret_cast<uint64 const*>(data.data() + size);
	objs.push_back_func(objCount, [&](size_t i) {
		return objIndexPtr[i];
	});
	offset = sizeof(uint64);
}
DeserContext::DeserContext() {
}
DeserContext::~DeserContext() {
}
}// namespace toolhub::ir