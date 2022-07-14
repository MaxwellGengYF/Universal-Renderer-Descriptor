#include "func_table.h"
namespace toolhub::unity {
FuncTable::FuncTable()
	: csharpMap(64), cppMap(64) {}
FuncTable::~FuncTable() {}
void FuncTable::SetCSharpFuncPtr(char const* name, CallbackFuncPtr ptr) {
	csharpMap.ForceEmplace(name, ptr);
}
CallbackFuncPtr FuncTable::GetCSharpFuncPtr(vstd::string_view name) {
	auto ite = csharpMap.Find(name);
	if (ite) return ite.Value();
	return nullptr;
}
void FuncTable::SetCppFuncPtr(vstd::string_view name, CallbackFuncPtr ptr) {
	cppMap.ForceEmplace(std::move(name), ptr);
}
CallbackFuncPtr FuncTable::GetCppFuncPtr(char const* name) {
	auto ite = cppMap.Find(name);
	if (ite) return ite.Value();
	return nullptr;
}
void callback_SetCSharpFuncPtr(FuncTable* callback, char const* name, CallbackFuncPtr ptr) {
	callback->SetCSharpFuncPtr(name, ptr);
}
CallbackFuncPtr callback_GetCppFuncPtr(FuncTable* callback, char const* name) {
	return callback->GetCppFuncPtr(name);
}
FuncTable* create_functable() {
	return new FuncTable();
}
void destroy_functable(FuncTable* table) {
	delete table;
}
}// namespace toolhub::unity