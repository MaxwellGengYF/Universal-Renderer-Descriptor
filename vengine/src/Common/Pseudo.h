#pragma once
#include <Common/Memory.h>
template<typename B, typename T>
struct VSTD_PseudoFunc {
	static_assert((std::is_base_of_v<B, T>)&&(sizeof(T) == sizeof(B)), "implement class must be empty");
	static void C(void* dst, T const* src) {
		if constexpr (std::is_copy_constructible_v<T>) {
			new (dst) T(*src);
		} else {
			VEngine_Log(typeid(T));
		}
	}
	static void M(void* dst, T* src) {
		if constexpr (std::is_move_constructible_v<T>) {
			new (dst) T(std::move(*src));
		} else {
			VEngine_Log(typeid(T));
		}
	}
};
namespace vstd {

class IPseudoObject : public IOperatorNewBase {
	template<typename I>
	requires(std::is_base_of_v<IPseudoObject, I>) friend class Pseudo;

protected:
	virtual void PseudoCopy(void* tarPtr) const& = 0;
	virtual void PseudoMove(void* tarPtr) && = 0;
};
#define PSEUDO_FUNC_IMPL(Base)                                                        \
	void PseudoCopy(void* tarPtr) const& override {                                   \
		VSTD_PseudoFunc<Base, std::remove_cvref_t<decltype(*this)>>::C(tarPtr, this); \
	}                                                                                 \
	void PseudoMove(void* tarPtr) && override {                                       \
		VSTD_PseudoFunc<Base, std::remove_cvref_t<decltype(*this)>>::M(tarPtr, this); \
	}

template<typename I>
requires(std::is_base_of_v<IPseudoObject, I>) class Pseudo : public IOperatorNewBase {
	std::aligned_storage_t<sizeof(I), alignof(I)> buffer;
	template<typename Func, typename... Args>
	static constexpr bool ValidFunction() {
		if constexpr ((std::is_same_v<std::remove_cvref_t<Func>, Pseudo>) || (std::is_same_v<std::remove_cvref_t<Func>, I>)) {
			return false;
		} else {
			return (std::is_invocable_v<Func, void*, Args...>);
		}
	}

public:
	I* operator->() { return reinterpret_cast<I*>(&buffer); }
	I const* operator->() const { return reinterpret_cast<I const*>(&buffer); }
	I* Get() { return reinterpret_cast<I*>(&buffer); }
	I const* Get() const { return reinterpret_cast<I const*>(&buffer); }
	I& operator*() & { return *Get(); }
	I const& operator*() const& { return *Get(); }
	I&& operator*() && { return std::move(*Get()); }
	template<typename Func, typename... Args>
	requires(ValidFunction<Func, Args&&...>())
		Pseudo(Func&& func, Args... args) {
		func(&buffer, std::forward<Args>(args)...);
	}
	Pseudo(Pseudo const& o) {
		o->PseudoCopy(&buffer);
	}
	Pseudo(Pseudo&& o) {
		std::move(*o).PseudoMove(&buffer);
	}
	Pseudo(I const& o) {
		o.PseudoCopy(&buffer);
	}
	Pseudo(I&& o) {
		std::move(o).PseudoMove(&buffer);
	}
	~Pseudo() {
		(*this)->~I();
	}
};
}// namespace vstd