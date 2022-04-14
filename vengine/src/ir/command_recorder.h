#pragma once
#include "Common/vstring.h"
#include <ir/statement.h>
#include <Utility/ObjectStackAlloc.h>
namespace luisa::ir {
class CommandRecorder : public vstd::IOperatorNewBase {
public:
	vstd::ObjectStackAlloc* objAlloc;
	enum class ScopeTag : vbyte {
		If,
		Loop,
		Function
	};
	using VarMap = vstd::HashMap<vstd::string, Var*>;
	struct Scope : public vstd::IOperatorNewBase {
		ScopeTag tag;
		vstd::vector<vstd::variant<
			Statement*,
			vstd::unique_ptr<Scope>>>
			stmts;
		VarMap varMap;
	};
	Var* TryGetVar(vstd::string_view name) const;
	void AddVar(Var* var, vstd::string_view name);
	Scope rootScope;
	VarMap globalVarMap;
	vstd::vector<Scope*> scopeStack;
	void Reset();
	void ClearFunction();
	void AddStmt(Statement* stmt);
	Scope* PushStack();
	Scope* LastStack();
	bool PopStack();
	CommandRecorder();
	~CommandRecorder();
};
}// namespace luisa::ir