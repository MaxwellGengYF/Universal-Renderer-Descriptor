#pragma once
#include <Common/MetaLib.h>
#include <Common/vstring.h>
namespace vstd {
struct VENGINE_DLL_COMMON Type {
private:
	vstd::string_view mName;
	size_t hashValue;

public:
	Type(const std::type_info& info) noexcept;
	Type() noexcept;
	Type(std::nullptr_t) noexcept : Type() {}
	int32 Compare(const Type& t) const noexcept;
	int32 Compare(const std::type_info& info) const noexcept;
	bool operator==(const Type& t) const noexcept;
	bool operator==(const std::type_info& t) const noexcept;
	bool operator!=(const Type& t) const noexcept { return !operator==(t); }
	bool operator!=(const std::type_info& t) const noexcept { return !operator==(t); }
	size_t HashCode() const noexcept { return hashValue; }
	vstd::string_view name() const noexcept { return mName; }
};
template<>
struct hash<Type> {
	size_t operator()(const Type& t) const noexcept {
		return t.HashCode();
	}
};
template<>
struct compare<Type> {
	int32 operator()(Type const& a, Type const& b) const {
		return a.Compare(b);
	}
	int32 operator()(Type const& a, std::type_info const& b) const {
		return a.Compare(b);
	}
};
struct AnyRef {
	void* ptr;
	Type typeName;
	template<typename T>
	AnyRef(T&& v)
		: ptr(&v), typeName(typeid(std::remove_cvref_t<T>)) {}
	template<typename T>
	requires(std::negation_v<std::is_reference<T>>)
	bool IsTypeOf() const {
		return typeName == typeid(T);
	}
	template<typename T>
	requires(std::negation_v<std::is_reference<T>>)
	T* Get() const {
		if (!IsTypeOf<T>()) return nullptr;
		return reinterpret_cast<T*>(ptr);
	}
};
}// namespace vstd