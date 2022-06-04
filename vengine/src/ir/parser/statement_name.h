#pragma once
#include <ir/parser/parser_utility.h>
#include <ir/api/statement.h>
#include <Utility/ObjectStackAlloc.h>
#include <ir/parser/type_descriptor.h>
namespace toolhub::ir {
class Parser;
class CommandRecorder;
class StatementName {
	vstd::optional<Var const*> PrepareVar(VarDescriptor const& varDesc);

public:
	struct CustomFunc {
		Type const* targetRetType;
		vstd::vector<Type const*> argsType;
		size_t index;
	};
	bool ExecuteCustomFunc(vstd::string_view funcName, CustomFunc const& func, vstd::span<Var const*> args, Var const* ret);

	// VarName, TypeName
	Parser* parser;
	CommandRecorder* recorder;
	vstd::ObjectStackAlloc* objAlloc;
	vstd::vector<Callable*> callables;
	using ArgSpan = vstd::span<vstd::string_view>;
	struct FuncCall {
		VarDescriptor varDesc;
		StatementName::ArgSpan args;
	};
	using FuncPtr = vstd::funcPtr_t<bool(StatementName&, FuncCall const&)>;
	vstd::HashMap<vstd::string_view, FuncPtr> funcs;
	vstd::vector<CustomFunc> customFuncs;
	vstd::HashMap<vstd::string, size_t> customFuncIndices;
	template<typename T>
	requires(std::is_enum_v<T>) using NameMap = vstd::HashMap<T, vstd::string>;
	NameMap<UnaryOp> unaryMap;
	NameMap<BinaryOp> binaryMap;
	NameMap<CallOp> callMap;
	void Init();
	bool Run(vstd::string_view funcName, VarDescriptor&& ret, ArgSpan sp);
	bool BuiltInFunc(CallOp callOp, FuncCall const& funcPack);
	bool BinaryOpCall(BinaryOp op, FuncCall const& funcPack);
	bool UnaryOpCall(UnaryOp op, FuncCall const& funcPack);
	bool GetMember(FuncCall const& funcPack);
	bool GetIndex(FuncCall const& funcPack);
	Callable const* AddCustomFunc(
		TypeDescriptor ret,
		vstd::string_view funcName,
		vstd::span<std::pair<vstd::string_view, Var const*>> args,
		vstd::vector<Statement const*>&& stmts);
	void Clear();
};
}// namespace toolhub::ir