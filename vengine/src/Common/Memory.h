#pragma once
#include <VEngineConfig.h>
#include <cstdlib>
#include <stdint.h>
#include <type_traits>
#include <Common/MetaLib.h>
VENGINE_C_FUNC_COMMON void* vengine_default_malloc(size_t sz);
VENGINE_C_FUNC_COMMON void vengine_default_free(void* ptr);
VENGINE_C_FUNC_COMMON void* vengine_default_realloc(void* ptr, size_t size);

VENGINE_C_FUNC_COMMON void* vengine_malloc(size_t size);
VENGINE_C_FUNC_COMMON void vengine_free(void* ptr);
VENGINE_C_FUNC_COMMON void* vengine_realloc(void* ptr, size_t size);
namespace vstd {
template<typename T = std::byte>
struct allocator {
	using value_type = T;
	constexpr allocator() noexcept = default;
	template<typename U>
	constexpr allocator(allocator<U>) noexcept {}
	[[nodiscard]] auto allocate(std::size_t n) const noexcept {
		return static_cast<T*>(vengine_malloc(sizeof(T) * n));
	}
	void deallocate(T* p, size_t) const noexcept {
		vengine_free(p);
	}
	template<typename R>
	[[nodiscard]] constexpr auto operator==(allocator<R>) const noexcept -> bool {
		return std::is_same_v<T, R>;
	}
};
}// namespace vstd
template<typename T, typename... Args>
requires(std::is_constructible_v<T, Args...>) inline T* vengine_new(Args&&... args) noexcept {
	T* tPtr = (T*)vengine_malloc(sizeof(T));
	new (tPtr) T(std::forward<Args>(args)...);
	return tPtr;
}

template<typename T, typename... Args>
requires(std::is_constructible_v<T, Args...>) inline T* vengine_new_array(size_t arrayCount, Args&&... args) noexcept {
	T* tPtr = (T*)vengine_malloc(sizeof(T) * arrayCount);
	for (auto&& i : vstd::ptr_range(tPtr, tPtr + arrayCount)) {
		new (&i) T(std::forward<Args>(args)...);
	}
	return tPtr;
}
template<typename T>
inline void vengine_delete(T* ptr) noexcept {
	if constexpr (!std::is_trivially_destructible_v<T>)
		((T*)ptr)->~T();
	vengine_free(ptr);
}
#define DECLARE_VENGINE_OVERRIDE_OPERATOR_NEW          \
	static void* operator new(                         \
		size_t size) noexcept {                        \
		return vengine_malloc(size);                   \
	}                                                  \
	static void* operator new(                         \
		size_t,                                        \
		void* place) noexcept {                        \
		return place;                                  \
	}                                                  \
	static void* operator new[](                       \
		size_t size) noexcept {                        \
		return vengine_malloc(size);                   \
	}                                                  \
	static void* operator new(                         \
		size_t size, const std::nothrow_t&) noexcept { \
		return vengine_malloc(size);                   \
	}                                                  \
	static void* operator new(                         \
		size_t,                                        \
		void* place, const std::nothrow_t&) noexcept { \
		return place;                                  \
	}                                                  \
	static void* operator new[](                       \
		size_t size, const std::nothrow_t&) noexcept { \
		return vengine_malloc(size);                   \
	}                                                  \
	static void operator delete(                       \
		void* pdead) noexcept {                        \
		vengine_free(pdead);                           \
	}                                                  \
	static void operator delete[](                     \
		void* pdead) noexcept {                        \
		vengine_free(pdead);                           \
	}

namespace vstd {
class IOperatorNewBase {
public:
	DECLARE_VENGINE_OVERRIDE_OPERATOR_NEW
};
class ISelfPtr : public IOperatorNewBase {
public:
	virtual ~ISelfPtr() = default;
	virtual void* SelfPtr() = 0;
};
#define VSTD_SELF_PTR \
	void* SelfPtr() override { return this; }
template<typename T>
struct DynamicObject {
	template<typename... Args>
	static constexpr T* CreateObject(
		funcPtr_t<T*(
			funcPtr_t<void*(size_t)> operatorNew,
			Args...)>
			createFunc,
		Args... args) {
		return createFunc(
			T::operator new,
			std::forward<Args>(args)...);
	}
};
}// namespace vstd
#define KILL_COPY_CONSTRUCT(clsName)  \
	clsName(clsName const&) = delete; \
	clsName& operator=(clsName const&) = delete;

#define KILL_MOVE_CONSTRUCT(clsName) \
	clsName(clsName&&) = delete;     \
	clsName& operator=(clsName&&) = delete;
