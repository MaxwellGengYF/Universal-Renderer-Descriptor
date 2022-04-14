#include "Common/vstring.h"
#include <ir/kernel.h>
#include <ir/command_recorder.h>
#include <ir/statement_name.h>
#include <ir/var_descriptor.h>
namespace luisa::ir {
class Parser {
	vstd::string errorMsg;
	vstd::unique_ptr<Kernel> kernel;
	vstd::HashMap<vstd::string, Type const*> customTypes;
	StatementName stateName;
	Type const* FindType(vstd::string_view name);
	vstd::vector<Var const*> functionVars;
	bool ParseStatement(
		char const*& ite,
		vstd::string_view& returnName,
		vstd::string_view& returnTypeName,
		vstd::string_view& stateName,
		vstd::vector<VarDescriptor>& argNames);
	bool ParseType(std::pair<vstd::string_view, size_t>& result, char const*& ite);

public:
	Parser();
	Type const* GetType(vstd::string_view typeName, size_t arraySize);
	bool ParseStruct(char const*& ite);
	bool ParseFunction(char const*& ite);
	bool ParseConst(char const*& ite);
	bool ParseGroupShared(char const*& ite);

	vstd::variant<
		vstd::unique_ptr<Kernel>,
		vstd::string>
	Parse(vstd::string const& str);
};
}// namespace luisa::ir