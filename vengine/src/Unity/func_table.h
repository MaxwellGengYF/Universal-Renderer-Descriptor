#pragma once
#include <Common/Common.h>
namespace toolhub::unity {
using CallbackFuncPtr = vstd::funcPtr_t<void(void* arg)>;
class FuncTable;
VENGINE_UNITY_EXTERN void callback_SetCSharpFuncPtr(FuncTable* callback, char const* name, CallbackFuncPtr ptr);
VENGINE_UNITY_EXTERN CallbackFuncPtr callback_GetCppFuncPtr(FuncTable* callback, char const* name);
VENGINE_UNITY_EXTERN FuncTable* create_functable();
VENGINE_UNITY_EXTERN void destroy_functable(FuncTable* table);
#ifdef VENGINE_UNITY_NATIVE
#define VENGINE_UNITY_CALLBACK_API __declspec(dllexport)
#else
#define VENGINE_UNITY_CALLBACK_API __declspec(dllimport)
#endif
class VENGINE_UNITY_CALLBACK_API FuncTable : public vstd::IOperatorNewBase{
	using Map = vstd::HashMap<vstd::string, CallbackFuncPtr>;
	Map csharpMap;
	Map cppMap;

	friend void callback_SetCSharpFuncPtr(FuncTable* callback, char const* name, CallbackFuncPtr ptr);
	friend CallbackFuncPtr callback_GetCppFuncPtr(FuncTable* callback, char const* name);
	void SetCSharpFuncPtr(char const* name, CallbackFuncPtr ptr);
	CallbackFuncPtr GetCppFuncPtr(char const* name);

public:
	FuncTable();
	~FuncTable();
	CallbackFuncPtr GetCSharpFuncPtr(vstd::string_view name);
	void SetCppFuncPtr(vstd::string_view name, CallbackFuncPtr ptr);
};
#define SET_CPP_FUNC_PTR(callback, func) callback->SetCppFuncPtr(#func##_sv, func)
#define GET_CSHARP_FUNC_PTR(callback, func) func = callback->GetCSharpFuncPtr(#func##_sv)
}// namespace toolhub::unity