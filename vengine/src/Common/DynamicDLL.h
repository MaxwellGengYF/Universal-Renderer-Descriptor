#pragma once
#include <VEngineConfig.h>
#include <Common/vstring.h>
#include <Common/Memory.h>
#include <Common/Log.h>
class VENGINE_DLL_COMMON DynamicDLL final : public vstd::IOperatorNewBase {
	size_t inst;
	template<typename T>
	struct IsFuncPtr {
		static constexpr bool value = false;
	};

	template<typename _Ret, typename... Args>
	struct IsFuncPtr<_Ret (*)(Args...)> {
		static constexpr bool value = true;
	};
	template<typename _Ret, typename... Args>
	struct IsFuncPtr<_Ret(Args...)> {
		static constexpr bool value = true;
	};
	size_t GetFuncPtr(char const* name) const;

public:
	DynamicDLL(char const* fileName);
	~DynamicDLL();
	DynamicDLL(DynamicDLL const&) = delete;
	DynamicDLL(DynamicDLL&&);
	template<typename T>
	void GetDLLFunc(T& funcPtr, char const* name) const {
		static_assert(IsFuncPtr<std::remove_cvref_t<T>>::value, "DLL Only Support Function Pointer!");
		auto ptr = GetFuncPtr(name);
		if (ptr == 0) {
			VEngine_Log(
				{"Can not find function ",
				 name});
			VENGINE_EXIT;
		}
		funcPtr = reinterpret_cast<T>(ptr);
	}
	template<typename T>
	vstd::funcPtr_t<T> GetDLLFunc(char const* name) const {
		static_assert(IsFuncPtr<std::remove_cvref_t<T>>::value, "DLL Only Support Function Pointer!");
		auto ptr = GetFuncPtr(name);
		if (ptr == 0) {
			VEngine_Log(
				{"Can not find function ",
				 name});
			VENGINE_EXIT;
		}
		return reinterpret_cast<vstd::funcPtr_t<T>>(ptr);
	}
};

template<typename FactoryType>
struct DllFactoryLoader {
private:
	vstd::optional<DynamicDLL> dll;
	vstd::funcPtr_t<FactoryType*()> funcPtr;

public:
	DllFactoryLoader(DllFactoryLoader const&) = delete;
	DllFactoryLoader(DllFactoryLoader&&) = delete;
	DllFactoryLoader(
		char const* dllName,
		char const* factoryFuncName) {
		dll.New(dllName);
		funcPtr = dll->GetDLLFunc<FactoryType*()>(factoryFuncName);
	}
	void UnloadDll() {
		dll.Delete();
	}
	DynamicDLL* GetDynamicDLL() {
		return dll;
	}
	FactoryType* operator()() const {
		return funcPtr();
	}
};