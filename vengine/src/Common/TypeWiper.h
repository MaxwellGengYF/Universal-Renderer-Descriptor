#pragma once
#include <typeinfo>
#include <type_traits>
#include <Common/MetaLib.h>
struct FunctorData {
	vstd::funcPtr_t<void(void*, const void*)> constructor;
	vstd::funcPtr_t<void(void*)> disposer;
	void (*run)(void*);
};

struct ObjData {
	vstd::funcPtr_t<void(void*)> disposer;
	void* ptr;
	template<typename T>
	ObjData(T* ptr)
		: ptr(ptr) {
		disposer = [](void* ptr) -> void {
			(reinterpret_cast<T*>(ptr))->~T();
		};
	}
};

template<typename Type>
FunctorData GetFunctor() {
	using T = typename std::remove_cvref_t<vstd::Type>;
	FunctorData data;
	data.constructor = [](void* place, const void* source) -> void {
		new (place) T(*(reinterpret_cast<T*>(source)));
	};
	data.disposer = [](void* ptr) -> void {
		(reinterpret_cast<T*>(ptr))->~T();
	};
	data.run = [](void* ptr) -> void {
		(*(reinterpret_cast<T*>(ptr)))();
	};
	return data;
}

template<typename Type, typename Ret, typename... Args>
Ret (*GetFunctorPointer())(void*, Args...) {
	using T = typename std::remove_cvref_t<vstd::Type>;
	return [](void* ptr, Args... args) -> Ret {
		return (*((T*)ptr))(args...);
	};
}

template<typename Type, typename Ret, typename... Args>
Ret (*GetFunctorPointer_Const())(void const*, Args...) {
	using T = typename std::remove_cvref_t<vstd::Type>;
	return [](void const* ptr, Args... args) -> Ret {
		return (*(reinterpret_cast<T const*>(ptr)))(args...);
	};
}