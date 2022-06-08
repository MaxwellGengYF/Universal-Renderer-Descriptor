#pragma once
#include <Common/Common.h>
#include <Common/functional.h>
namespace toolhub::ir {
//using PushFunc = vstd::function<void(void const*, size_t)>;
//using PopFunc = vstd::function<void(void*, size_t)>;
class SerContext;
class DeserContext;
/*struct Serializer {
	friend class SerContext;
	friend class DeserContext;

protected:
	virtual void Ser(SerContext& func) const = 0;
	virtual void Deser(DeserContext& ptr) = 0;
	~Serializer() = default;
};*/
template<typename T>
concept SerializerField = requires(T t, T const tConst, SerContext& func, DeserContext& ptr) {
	std::is_same_v<decltype(tConst.Ser(func)), void>;
	std::is_same_v<decltype(t.Deser(ptr)), void>;
};
struct TrivialSerializable {};
template<typename T>
static constexpr bool trivialSerializable = std::is_trivial_v<T> || std::is_base_of_v<TrivialSerializable, T>;
};// namespace toolhub::ir