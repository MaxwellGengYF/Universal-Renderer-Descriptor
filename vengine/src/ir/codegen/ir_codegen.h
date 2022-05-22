#pragma once
#include <ir/codegen/codegen.h>
namespace toolhub::ir {
class IRCodegen : public Codegen {
	vstd::HashMap<Type const*, vstd::string> typeNames;
	void PrintType(Type const* t, vstd::string& str);
	void GetTypeName(Type const* t, vstd::string& str);
	void PrintConst(vstd::span<ConstantVar const> vars, vstd::string& str);
	vstd::string GetTypeName();

public:
	~IRCodegen();
	vstd::string Gen(Kernel const& kernel) override;
};
};// namespace toolhub::ir