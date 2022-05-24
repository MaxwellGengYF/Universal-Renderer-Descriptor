#pragma once
#include <ir/codegen/codegen.h>
namespace toolhub::ir {
class IRCodegen : public Codegen {
	vstd::HashMap<ConstantVar const*, size_t> constIndices;
	vstd::HashMap<Type const*, vstd::string> typeNames;
	vstd::HashMap<Var const*, size_t> varMap;
	vstd::vector<vstd::string_view> callOpNames;
	vstd::vector<vstd::string_view> binOpNames;
	vstd::vector<vstd::string_view> unaOpNames;
	void PrintType(Type const* t, vstd::string& str);
	void GetTypeName(Type const* t, vstd::string& str);
	void PrintConst(vstd::span<ConstantVar const* const> vars, vstd::string& str);
	void PrintFunc(Callable const& callable, vstd::string& str, size_t index);
	void PrintStatement(Statement const* stmt, vstd::string& str);
	void PrintVar(Var const* var, vstd::string& str);
	void InitStatement();
	vstd::string GetTypeName();

public:
	IRCodegen(){
		InitStatement();
	}
	~IRCodegen();
	vstd::string Gen(Kernel const& kernel) override;
	template<typename Op>
	void RegistState(Op op, vstd::string_view strv);
};
};// namespace toolhub::ir