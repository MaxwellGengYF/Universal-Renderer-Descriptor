#pragma once
#include <ir/api/statement.h>
#include <Utility/ObjectStackAlloc.h>
namespace toolhub::ir {

struct IfScope;
struct LoopScope;
struct FuncScope;
struct SwitchScope;
struct CaseScope;
struct Scope;
enum class ScopeTag : vbyte {
	If,
	Loop,
	Function,
	Switch,
	Case
};
using VarMap = vstd::HashMap<vstd::string, Var const*>;
struct Scope : public vstd::IOperatorNewBase {
	VarMap varMap;
	virtual Statement const* ToStatement(vstd::ObjectStackAlloc& alloc) = 0;
	virtual bool emplace(Statement const* stmt) = 0;
	virtual ScopeTag tag() const = 0;
	virtual ~Scope() = default;
};
struct IfScope : public Scope {
	vstd::vector<Statement const*> trueStmts;
	vstd::vector<Statement const*> falseStmts;
	bool state = true;
	Var const* condition;
	ScopeTag tag() const override { return ScopeTag::If; }
	bool emplace(Statement const* stmt) override;
	Statement const* ToStatement(vstd::ObjectStackAlloc& alloc) override;
};
struct CaseScope : public Scope {
	vstd::vector<Statement const*> stmts;
	int64 index;
	ScopeTag tag() const override { return ScopeTag::Case; }
	bool emplace(Statement const* stmt) override;
	Statement const* ToStatement(vstd::ObjectStackAlloc& alloc) override { return nullptr; }
};
struct SwitchScope : public Scope {
	vstd::vector<vstd::unique_ptr<CaseScope>> cases;
	Var const* condition;
	ScopeTag tag() const override { return ScopeTag::Switch; }
	bool emplace(Statement const* stmt) override;
	Statement const* ToStatement(vstd::ObjectStackAlloc& alloc) override;
};
struct LoopScope : public Scope {
	vstd::vector<Statement const*> stmts;
	Var const* condition;
	ScopeTag tag() const override { return ScopeTag::Loop; }
	bool emplace(Statement const* stmt) override;
	Statement const* ToStatement(vstd::ObjectStackAlloc& alloc) override;
};
struct FuncScope : public Scope {
	vstd::vector<Statement const*> stmts;
	ScopeTag tag() const override { return ScopeTag::Function; }
	bool emplace(Statement const* stmt) override;
	Statement const* ToStatement(vstd::ObjectStackAlloc& alloc) override { return nullptr; }
};
class CommandRecorder : public vstd::IOperatorNewBase {
public:
	vstd::ObjectStackAlloc* objAlloc;
	Var const* TryGetVar(vstd::string_view name) const;
	void AddVar(Var const* var, vstd::string_view name);
	VarMap globalVarMap;
	vstd::vector<vstd::unique_ptr<Scope>> scopeStack;
	void Reset();
	void ClearFunction();
	void AddStmt(Statement const* stmt);
	Scope* PushStack(ScopeTag tag);
	Scope* NewStack(ScopeTag tag);
	Scope* LastStack();
	bool PopStack();
	CommandRecorder();
	~CommandRecorder();
	vstd::vector<Statement const*>&& GetStatements();
	void AddArguments(vstd::span<std::pair<vstd::string_view, Var const*>> vars);
};
}// namespace toolhub::ir