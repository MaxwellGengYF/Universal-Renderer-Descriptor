
#include "Common/AllocateType.h"
#include "Common/vector.h"
#include <ir/command_recorder.h>
namespace luisa::ir {
void CommandRecorder::AddStmt(Statement* stmt) {
	LastStack()->stmts.emplace_back(stmt);
}
CommandRecorder::Scope* CommandRecorder::PushStack() {
	auto newScope = new Scope();
	(*scopeStack.last())->stmts.emplace_back(vstd::create_unique(newScope));
	scopeStack.emplace_back(newScope);
	return newScope;
}
bool CommandRecorder::PopStack() {
	if (scopeStack.size() <= 1) return false;
	scopeStack.erase_last();
	return true;
}
CommandRecorder::CommandRecorder() {
	rootScope.tag = ScopeTag::Function;
	scopeStack.emplace_back(&rootScope);
}
CommandRecorder::~CommandRecorder() {
}
CommandRecorder::Scope* CommandRecorder::LastStack() {
	return *scopeStack.last();
}
void CommandRecorder::Reset() {
	globalVarMap.Clear();
	ClearFunction();
}
void CommandRecorder::ClearFunction() {
	scopeStack.resize(1);
	rootScope.stmts.clear();
}
Var* CommandRecorder::TryGetVar(vstd::string_view name) const {
	for (auto ite = scopeStack.rcbegin(); ite != scopeStack.rcend(); --ite) {
		auto result = (*ite)->varMap.Find(name);
		if (result) return result.Value();
	}
	auto globalResult = globalVarMap.Find(name);
	if (globalResult) return globalResult.Value();
}
void CommandRecorder::AddVar(Var* var, vstd::string_view name) {
	(*scopeStack.last())->varMap.Emplace(name, var);
}

}// namespace luisa::ir