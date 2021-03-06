#pragma once
#include <Common/vstring.h>
#include <ir/api/kernel.h>
#include <ir/parser/command_recorder.h>
#include <ir/parser/statement_name.h>
#include <ir/parser/type_descriptor.h>
namespace toolhub::ir {
class Parser {
	vstd::string errorMsg;
	vstd::unique_ptr<Kernel> kernel;
	vstd::HashMap<vstd::string, Type const*> customTypes;
	StatementName stateName;
	vstd::vector<std::pair<vstd::string_view, Var const*>> functionVars;
	CommandRecorder recorder;
	uint64_t varIndex = 0;
	bool ParseStatement(
		char const*& ite,
		vstd::string_view& returnName,
		TypeDescriptor& typeDesc,
		vstd::string_view& stateName,
		vstd::vector<vstd::string_view>& argNames);
	bool ParseType(TypeDescriptor& result, char const*& ite);

public:
	Parser();
	Type const* GetType(TypeDescriptor const& desc);
	bool ParseStruct(char const*& ite);
	bool ParseFunction(char const*& ite);
	bool ParseConst(char const*& ite);
	bool ParseGroupShared(char const*& ite);

	vstd::variant<
		vstd::unique_ptr<Kernel>,
		vstd::string>
	Parse(vstd::string const& str);
	uint64_t GetVarIndex() {
		return varIndex++;
	}
};
}// namespace toolhub::ir