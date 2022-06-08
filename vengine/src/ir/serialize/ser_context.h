#pragma once
#include <ir/serialize/serializer.h>
namespace toolhub::ir {
class SerContext {
	struct PlaceHolder {
		uint64 index;
		uint64 offset;
	};
	using SerFunc = vstd::funcPtr_t<void(void const*, SerContext&)>;
	using ObjMap = vstd::HashMap<void const*, PlaceHolder>;
	struct Command {
		void const* ptr;
		SerFunc SerFunc;
		ObjMap::Index idx;
	};
	ObjMap map;
	vstd::vector<Command> vec[2];
	bool swapChain = false;
	uint64 index = 0;
	vstd::vector<vbyte> serResult;
	SerContext(SerContext const&) = delete;
	void Clear();
	uint64 AddSerObj(void const* ser, SerFunc SerFunc);
	void ProcessSerialize();

public:
	vstd::vector<vbyte> const& data() const& { return serResult; }
	vstd::vector<vbyte>&& data() && { return std::move(serResult); }
	SerContext() {}

	template<SerializerField T>
	uint64 AddSerObj(T const* ptr) {
		return AddSerObj(ptr, [](void const* ptr, auto... context) {
			reinterpret_cast<T const*>(ptr)->Ser(context...);
		});
	}

	template<typename T>
	requires(trivialSerializable<T> && !std::is_pointer_v<T>) void Push(T const& data) {
		auto sz = serResult.size();
		serResult.resize(serResult.size() + sizeof(T));
		memcpy(serResult.data() + sz, &data, sizeof(T));
	}
	template<SerializerField T>
	void Push(T const* t) {
		Push(AddSerObj(t));
	}
	bool UpdateCmd();
	~SerContext();
	template<SerializerField T>
	void Serialize(T const* root) {
		Clear();
		serResult.resize(sizeof(uint64));
		root->Ser(*this);
		ProcessSerialize();
	}
};
}// namespace toolhub::ir