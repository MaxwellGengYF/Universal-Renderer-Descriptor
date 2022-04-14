#pragma once
#include <ir/parser_utility.h>
#include <ir/statement.h>
#include <ir/command_recorder.h>
#include <Utility/ObjectStackAlloc.h>
#include <ir/var_descriptor.h>
namespace luisa::ir {
class Parser;
class StatementName {
	void CustomInit();

public:
	// VarName, TypeName
	Parser* parser;
	using ArgSpan = vstd::span<VarDescriptor const>;
	struct FuncCall {
		VarDescriptor ret;
		StatementName::ArgSpan args;
	};
	using FuncPtr = vstd::funcPtr_t<bool(StatementName&, FuncCall const&)>;
	vstd::HashMap<vstd::string_view, FuncPtr> funcs;
	template<typename T>
	requires(std::is_enum_v<T>) using NameMap = vstd::HashMap<T, vstd::string>;
	NameMap<UnaryOp> unaryMap;
	NameMap<BinaryOp> binaryMap;
	NameMap<CallOp> callMap;
	CommandRecorder recorder;
	void Init();
	bool operator()(vstd::string_view funcName, VarDescriptor const& retType, ArgSpan sp) {
		auto ite = funcs.Find(funcName);
		if (!ite) return false;
		auto&& v = ite.Value();
		return v(*this, FuncCall{retType, sp});
	}
	bool BuiltInFunc(CallOp callOp, FuncCall const& funcPack);
	bool BinaryOpCall(BinaryOp op, FuncCall const& funcPack);
	bool UnaryOpCall(UnaryOp op, FuncCall const& funcPack);
};
}// namespace luisa::ir