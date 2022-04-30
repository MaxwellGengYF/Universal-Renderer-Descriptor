#pragma once
#include <ir/parser/parser_utility.h>
#include <ir/api/statement.h>
#include <Utility/ObjectStackAlloc.h>
#include <ir/parser/type_descriptor.h>
namespace luisa::ir {
class Parser;
class CommandRecorder;
class StatementName {
	Var const* PrepareVar(VarDescriptor const& varDesc);
public:
	struct CustomFunc {
		Type const* targetRetType;
		vstd::vector<Type const*> argsType;
	};
	bool ExecuteCustomFunc(CustomFunc const& func, vstd::span<Var const*> args, Var const* ret);

	// VarName, TypeName
	Parser* parser;
	CommandRecorder* recorder;
	vstd::ObjectStackAlloc* objAlloc;
	using ArgSpan = vstd::span<vstd::string_view>;
	struct FuncCall {
		VarDescriptor varDesc;
		StatementName::ArgSpan args;
	};
	using FuncPtr = vstd::funcPtr_t<bool(StatementName&, FuncCall const&)>;
	vstd::HashMap<vstd::string_view, FuncPtr> funcs;
	vstd::HashMap<vstd::string_view, CustomFunc> customFuncs;
	template<typename T>
	requires(std::is_enum_v<T>) using NameMap = vstd::HashMap<T, vstd::string>;
	NameMap<UnaryOp> unaryMap;
	NameMap<BinaryOp> binaryMap;
	NameMap<CallOp> callMap;
	void Init();
	bool operator()(vstd::string_view funcName, VarDescriptor const& ret, ArgSpan sp);
	bool BuiltInFunc(CallOp callOp, FuncCall const& funcPack);
	bool BinaryOpCall(BinaryOp op, FuncCall const& funcPack);
	bool UnaryOpCall(UnaryOp op, FuncCall const& funcPack);
	bool AddCustomFunc(TypeDescriptor ret, vstd::string_view funcName, vstd::span<TypeDescriptor> args);
	void Clear();
};
}// namespace luisa::ir