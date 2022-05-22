
#include "Common/AllocateType.h"
#include "Common/vector.h"
#include <ir/parser/command_recorder.h>
namespace toolhub::ir {
void CommandRecorder::AddStmt(Statement const* stmt) {
	LastStack()->emplace(stmt);
}
Scope* CommandRecorder::NewStack(ScopeTag tag) {
	Scope* newScope;
	switch (tag) {
		case ScopeTag::Function:
			newScope = new FuncScope();
			break;
		case ScopeTag::If:
			newScope = new IfScope();
			break;
		case ScopeTag::Loop:
			newScope = new LoopScope();
			break;
		default:
			newScope = nullptr;
			break;
	}
	return newScope;
}

Scope* CommandRecorder::PushStack(ScopeTag tag) {
	auto newScope = NewStack(tag);
	scopeStack.emplace_back(newScope);
	return newScope;
}
bool CommandRecorder::PopStack() {
	if (scopeStack.size() <= 1) return false;
	auto scope = scopeStack.erase_last();
	if(!scopeStack.empty()){
		auto&& last = *scopeStack.last();
		auto stmt = scope->ToStatement(*objAlloc);
		if(stmt){
			if(!last->emplace(stmt)) return false;
		}
	}
	return true;
}
CommandRecorder::CommandRecorder() {
	scopeStack.emplace_back(new FuncScope());
}
CommandRecorder::~CommandRecorder() {
}
Scope* CommandRecorder::LastStack() {
	if (scopeStack.empty()) return nullptr;
	return scopeStack.last()->get();
}
void CommandRecorder::Reset() {
	globalVarMap.Clear();
	ClearFunction();
}
void CommandRecorder::ClearFunction() {
	scopeStack.resize(1);
	vstd::reset(static_cast<FuncScope*>(scopeStack[0].get())->stmts);
}
Var const* CommandRecorder::TryGetVar(vstd::string_view name) const {
	for (auto ite = scopeStack.rbegin(); ite != scopeStack.rend(); --ite) {
		auto result = (*ite)->varMap.Find(name);
		if (result) return result.Value();
	}
	auto globalResult = globalVarMap.Find(name);
	if (globalResult) return globalResult.Value();
	return nullptr;
}
void CommandRecorder::AddVar(Var const* var, vstd::string_view name) {
	(*scopeStack.last())->varMap.Emplace(name, var);
}
bool IfScope::emplace(Statement const* stmt) {
	if (state) {
		trueStmts.emplace_back(std::move(stmt));
	} else {
		falseStmts.emplace_back(std::move(stmt));
	}
	return true;
}
bool LoopScope::emplace(Statement const* stmt) {
	stmts.emplace_back(std::move(stmt));
	return true;
}
bool FuncScope::emplace(Statement const* stmt) {
	stmts.emplace_back(std::move(stmt));
	return true;
}
bool SwitchScope::emplace(Statement const* stmt) {
	return false;
}
bool CaseScope::emplace(Statement const* stmt) {
	stmts.emplace_back(stmt);
	return false;
}
Statement const* IfScope::ToStatement(vstd::ObjectStackAlloc& alloc) {
	auto stmt = alloc.Allocate<IfStmt>();
	stmt->condition = condition;
	stmt->trueField = std::move(trueStmts);
	stmt->falseField= std::move(falseStmts);
	return stmt;
}
Statement const* SwitchScope::ToStatement(vstd::ObjectStackAlloc& alloc) {
	auto stmt = alloc.Allocate<SwitchStmt>();
	stmt->cases.push_back_func(cases.size(), [&](size_t i){
		auto&& c = cases[i];
		SwitchStmt::Case dst;
		dst.first = c->index;
		dst.second = std::move(c->stmts);
		return dst;
	});
	return stmt;
}
Statement const* LoopScope::ToStatement(vstd::ObjectStackAlloc &alloc){
	auto stmt = alloc.Allocate<LoopStmt>();
	stmt->condition = condition;
	stmt->commands = std::move(stmts);
	return stmt;	
}
vstd::vector<Statement const*>&& CommandRecorder::GetStatements(){
	auto funcScope = static_cast<FuncScope*>(scopeStack[0].get());
	return std::move(funcScope->stmts);
}
void CommandRecorder::AddArguments(vstd::span<std::pair<vstd::string_view, const Var *>> vars){
	auto&& varMap = static_cast<FuncScope*>(scopeStack[0].get())->varMap;
	for(auto&& i : vars){
		varMap.Emplace(i.first, i.second);
	}
}
}// namespace toolhub::ir