#pragma once
#include <ir/serialize/serializer.h>
#include <Utility/ObjectStackAlloc.h>
namespace toolhub::ir {
class DeserContext {
	vstd::span<vbyte const> data;
	vstd::vector<vstd::variant<uint64, void*>> objs;
	vstd::ObjectStackAlloc* alloc;
	uint64 offset;
	void InitDeser(
		vstd::span<vbyte const> data,
		vstd::ObjectStackAlloc* alloc);

public:
	template<typename T>
	requires(trivialSerializable<T>) void Pop(T& data) {
		memcpy(&data, Data(), sizeof(T));
		offset += sizeof(T);
	}

	vbyte const* Data() const { return data.data() + offset; }
	DeserContext();
	~DeserContext();
	template<SerializerField T>
	void DeSerialize(
		vstd::span<vbyte const> data,
		vstd::ObjectStackAlloc* alloc,
		T* rootObj) {
		InitDeser(data, alloc);
		rootObj->Deser(*this);
	}
	template<SerializerField T>
	T* TryDeser() {
		uint64 index;
		Pop(index);
		auto&& ite = objs[index];
		return ite.multi_visit_or(
			(T*)nullptr,
			[&](uint64 value) {
				auto ptr = alloc->Allocate<T>();
				auto lastOffset = offset;
				offset = value;
				reinterpret_cast<T*>(ptr)->Deser(*this);
				offset = lastOffset;
				ite = ptr;
				return ptr;
			},
			[&](void* value) {
				return reinterpret_cast<T*>(value);
			});
	}
};
}// namespace toolhub::ir