#pragma once
#include <VEngineConfig.h>
#include <stdint.h>
#include <Common/Hash.h>
#include <Common/Memory.h>
#include <type_traits>
#include <new>
#include <Common/VAllocator.h>
namespace vstd {
static constexpr size_t FUNCTION_PLACEHOLDERSIZE = 40;

/*
template<class T, VEngine_AllocType allocType = VEngine_AllocType::VEngine>
class function;
template<class Ret,
		 class... TypeArgs, VEngine_AllocType allocType>
class function<Ret(TypeArgs...), allocType> {
	using Allocator = VAllocHandle<allocType>;
	template<typename Call, VEngine_AllocType>
	friend struct LazyCallback;
	/////////////////////Define
	using PlaceHolderType = std::aligned_storage_t<FUNCTION_PLACEHOLDERSIZE, alignof(size_t)>;
	struct IProcessFunctor {
		virtual void Delete(void* ptr) const = 0;
		virtual void CopyConstruct(void*& dst, void const* src, PlaceHolderType* holder) const = 0;
		virtual void MoveConstruct(void*& dst, void* src, PlaceHolderType* holder) const = 0;
	};
	using ProcessorHolder = std::aligned_storage_t<sizeof(IProcessFunctor), alignof(IProcessFunctor)>;
	/////////////////////Data
	void* placePtr;
	funcPtr_t<Ret(void*, TypeArgs&&...)> runFunc;
	ProcessorHolder logicPlaceHolder;
	PlaceHolderType funcPtrPlaceHolder;
	IProcessFunctor const* GetPtr() const noexcept {
		return reinterpret_cast<IProcessFunctor const*>(&logicPlaceHolder);
	}
	template<typename Func>
	static constexpr bool LegalFunction() {
		if constexpr (std::is_same_v<std::remove_cvref_t<Func>, function>) {
			return false;
		} else {
			return std::is_invocable_r_v<Ret, Func, TypeArgs...>;
		}
	}

public:
	operator bool() const noexcept {
		return placePtr;
	}
	bool operator!() const {
		return placePtr == nullptr;
	}
	void Dispose() noexcept {
		if (placePtr) {
			GetPtr()->Delete(placePtr);
			placePtr = nullptr;
		}
		runFunc = nullptr;
	}
	function() noexcept
		: placePtr(nullptr),
		  runFunc(nullptr) {
	}
	function(std::nullptr_t) noexcept
		: function() {
	}
	function(funcPtr_t<Ret(TypeArgs...)> p) noexcept {
		struct FuncPtrLogic final : public IProcessFunctor {
			void Delete(void* ptr) const override {
			}
			void CopyConstruct(void*& dest, void const* source, PlaceHolderType* placeHolder) const override {
				reinterpret_cast<size_t&>(dest) = reinterpret_cast<size_t>(source);
			}
			void MoveConstruct(void*& dst, void* src, PlaceHolderType* placeHolder) const override {
				reinterpret_cast<size_t&>(dst) = reinterpret_cast<size_t>(src);
			}
		};
		runFunc = [](void* pp, TypeArgs&&... tt) -> Ret {
			funcPtr_t<Ret(TypeArgs...)> fp = reinterpret_cast<funcPtr_t<Ret(TypeArgs...)>>(const_cast<void*>(pp));
			return fp(std::forward<TypeArgs>(tt)...);
		};
		new (&logicPlaceHolder) FuncPtrLogic();
		reinterpret_cast<size_t&>(placePtr) = reinterpret_cast<size_t>(p);
	}
	function(const function& f) noexcept
		: logicPlaceHolder(f.logicPlaceHolder),
		  runFunc(f.runFunc) {
		if (f.placePtr) {
			GetPtr()->CopyConstruct(placePtr, f.placePtr, &funcPtrPlaceHolder);
		}
	}
	function(function&& f) noexcept
		: logicPlaceHolder(f.logicPlaceHolder),
		  runFunc(f.runFunc) {
		if (f.placePtr) {
			GetPtr()->MoveConstruct(placePtr, f.placePtr, &funcPtrPlaceHolder);
			f.placePtr = nullptr;
		}
		f.runFunc = nullptr;
	}
	template<typename Functor>
	requires(LegalFunction<Functor>())
		function(Functor&& f)
	noexcept {
		using PureType = std::remove_cvref_t<Functor>;
		constexpr bool USE_HEAP = (sizeof(PureType) > FUNCTION_PLACEHOLDERSIZE);
		struct FuncPtrLogic final : public IProcessFunctor {
			void Delete(void* pp) const override {
				PureType* ff = (PureType*)pp;
				ff->~PureType();
				if constexpr (USE_HEAP) {
					Allocator().Free(pp);
				}
			}
			void CopyConstruct(void*& dest, void const* source, PlaceHolderType* placeHolder) const override {
				if constexpr (!std::is_copy_constructible_v<PureType>) {
					VEngine_Log(typeid(PureType));
					VENGINE_EXIT;
				} else {
					if constexpr (USE_HEAP) {
						dest = Allocator().Malloc(sizeof(PureType));
					} else {
						dest = placeHolder;
					}
					new (dest) PureType(*(PureType const*)source);
				}
			}
			void MoveConstruct(void*& dest, void* source, PlaceHolderType* placeHolder) const override {
				if constexpr (!std::is_move_constructible_v<PureType>) {
					VEngine_Log(typeid(PureType));
					VENGINE_EXIT;
				} else {
					if constexpr (USE_HEAP) {
						dest = source;
					} else {
						dest = placeHolder;
						new (dest) PureType(
							std::move(*reinterpret_cast<PureType*>(source)));
					}
				}
			}
		};
		runFunc = [](void* pp, TypeArgs&&... tt) -> Ret {
			PureType* ff = reinterpret_cast<PureType*>(pp);
			return (*ff)(std::forward<TypeArgs>(tt)...);
		};
		new (&logicPlaceHolder) FuncPtrLogic();
		if constexpr (USE_HEAP) {
			placePtr = Allocator().Malloc(sizeof(PureType));
		} else {
			placePtr = &funcPtrPlaceHolder;
		}
		new (placePtr) PureType(std::forward<Functor>(f));
	}
	void operator=(std::nullptr_t) noexcept {
		Dispose();
	}
	template<typename Functor>
	void operator=(Functor&& f) noexcept {
		this->~function();
		new (this) function(std::forward<Functor>(f));
	}
	Ret operator()(TypeArgs... t) const noexcept {
		return runFunc(placePtr, std::forward<TypeArgs>(t)...);
	}
	~function() noexcept {
		if (placePtr) {
			GetPtr()->Delete(placePtr);
		}
	}
};

*/
template<class T, VEngine_AllocType allocType = VEngine_AllocType::VEngine>
class function;
template<class Ret,
		 class... TypeArgs, VEngine_AllocType allocType>
class function<Ret(TypeArgs...), allocType> {
	using Allocator = VAllocHandle<allocType>;
	template<typename Call, VEngine_AllocType>
	friend struct LazyCallback;
	/////////////////////Define
	using PlaceHolderType = std::aligned_storage_t<FUNCTION_PLACEHOLDERSIZE, alignof(size_t)>;
	struct IProcessFunctor {
		virtual void Delete(void* ptr) const = 0;
		virtual void MoveConstruct(void*& dst, void* src, PlaceHolderType* holder) const = 0;
	};
	using ProcessorHolder = std::aligned_storage_t<sizeof(IProcessFunctor), alignof(IProcessFunctor)>;
	/////////////////////Data
	void* placePtr;
	funcPtr_t<Ret(void*, TypeArgs&&...)> runFunc;
	ProcessorHolder logicPlaceHolder;
	PlaceHolderType funcPtrPlaceHolder;
	IProcessFunctor const* GetPtr() const noexcept {
		return reinterpret_cast<IProcessFunctor const*>(&logicPlaceHolder);
	}
	template<typename Func>
	static constexpr bool LegalFunction() {
		if constexpr (std::is_same_v<std::remove_cvref_t<Func>, function>) {
			return false;
		} else {
			return std::is_invocable_r_v<Ret, Func, TypeArgs...>;
		}
	}

public:
	operator bool() const noexcept {
		return placePtr;
	}
	bool operator!() const {
		return placePtr == nullptr;
	}
	void Dispose() noexcept {
		if (placePtr) {
			GetPtr()->Delete(placePtr);
			placePtr = nullptr;
		}
		runFunc = nullptr;
	}
	function() noexcept
		: placePtr(nullptr),
		  runFunc(nullptr) {
	}
	function(std::nullptr_t) noexcept
		: function() {
	}
	function(funcPtr_t<Ret(TypeArgs...)> p) noexcept {
		struct FuncPtrLogic final : public IProcessFunctor {
			void Delete(void* ptr) const override {
			}
			void MoveConstruct(void*& dst, void* src, PlaceHolderType* placeHolder) const override {
				reinterpret_cast<size_t&>(dst) = reinterpret_cast<size_t>(src);
			}
		};
		runFunc = [](void* pp, TypeArgs&&... tt) -> Ret {
			funcPtr_t<Ret(TypeArgs...)> fp = reinterpret_cast<funcPtr_t<Ret(TypeArgs...)>>(const_cast<void*>(pp));
			return fp(std::forward<TypeArgs>(tt)...);
		};
		new (&logicPlaceHolder) FuncPtrLogic();
		reinterpret_cast<size_t&>(placePtr) = reinterpret_cast<size_t>(p);
	}
	function(const function& f) noexcept = delete;
	function(function&& f) noexcept
		: logicPlaceHolder(f.logicPlaceHolder),
		  runFunc(f.runFunc) {
		if (f.placePtr) {
			GetPtr()->MoveConstruct(placePtr, f.placePtr, &funcPtrPlaceHolder);
			f.placePtr = nullptr;
		}
		f.runFunc = nullptr;
	}
	template<typename Functor>
	requires(LegalFunction<Functor>())
		function(Functor&& f)
	noexcept {
		using PureType = std::remove_cvref_t<Functor>;
		constexpr bool USE_HEAP = (sizeof(PureType) > FUNCTION_PLACEHOLDERSIZE);
		struct FuncPtrLogic final : public IProcessFunctor {
			void Delete(void* pp) const override {
				PureType* ff = (PureType*)pp;
				ff->~PureType();
				if constexpr (USE_HEAP) {
					Allocator().Free(pp);
				}
			}
			void MoveConstruct(void*& dest, void* source, PlaceHolderType* placeHolder) const override {
				if constexpr (!std::is_move_constructible_v<PureType>) {
					VEngine_Log(typeid(PureType));
					VENGINE_EXIT;
				} else {
					if constexpr (USE_HEAP) {
						dest = source;
					} else {
						dest = placeHolder;
						new (dest) PureType(
							std::move(*reinterpret_cast<PureType*>(source)));
					}
				}
			}
		};
		runFunc = [](void* pp, TypeArgs&&... tt) -> Ret {
			PureType* ff = reinterpret_cast<PureType*>(pp);
			return (*ff)(std::forward<TypeArgs>(tt)...);
		};
		new (&logicPlaceHolder) FuncPtrLogic();
		if constexpr (USE_HEAP) {
			placePtr = Allocator().Malloc(sizeof(PureType));
		} else {
			placePtr = &funcPtrPlaceHolder;
		}
		new (placePtr) PureType(std::forward<Functor>(f));
	}
	void operator=(std::nullptr_t) noexcept {
		Dispose();
	}
	template<typename Functor>
	void operator=(Functor&& f) noexcept {
		this->~function();
		new (this) function(std::forward<Functor>(f));
	}
	Ret operator()(TypeArgs... t) const noexcept {
		return runFunc(placePtr, std::forward<TypeArgs>(t)...);
	}
	~function() noexcept {
		if (placePtr) {
			GetPtr()->Delete(placePtr);
		}
	}
};

template<typename Call, VEngine_AllocType allocType = VEngine_AllocType::VEngine>
struct LazyCallback;

template<typename R1, typename... A1, VEngine_AllocType allocType>
class LazyCallback<R1(A1...), allocType> {
private:
	vstd::function<R1(A1...), allocType> callBack;
	vstd::function<void()> disposer;

public:
	template<typename A, typename B>
	requires(
		std::is_constructible_v<vstd::function<R1(A1...)>, A&&> || std::is_constructible_v<vstd::function<void()>, B&&>)
		LazyCallback(A&& a, B&& b)
		: callBack(std::forward<A>(a)),
		  disposer(std::forward<B>(b)) {}
	R1 operator()(A1... a) const {
		return callBack.runFunc(callBack.placePtr, std::forward<A1>(a)...);
	}
	LazyCallback(LazyCallback const&) = delete;
	LazyCallback(LazyCallback&&) = default;
	~LazyCallback() {
		if (disposer) disposer();
	}
};
template<typename Vec, typename Func>
void push_back_func(Vec&& vec, Func&& func, size_t count) {
	std::fill_n(
		std::back_inserter(vec),
		count,
		LazyEval<std::remove_cvref_t<Func>>(std::forward<Func>(func)));
}
}// namespace vstd