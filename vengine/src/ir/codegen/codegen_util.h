#pragma once
#include <ir/api/var.h>
#include <ir/api/statement.h>
namespace toolhub::ir {
class CodegenUtil {
	CodegenUtil() = delete;
	CodegenUtil(CodegenUtil&&) = delete;
	CodegenUtil(CodegenUtil const&) = delete;

public:
	static void PrintConstArray(ConstantVar const& var, vstd::string& str);
};
}// namespace toolhub::ir