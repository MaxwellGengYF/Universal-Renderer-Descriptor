#include <Common/vstring.h>
#include <ir/api/kernel.h>
#include <ir/parser/command_recorder.h>
#include <ir/parser/statement_name.h>
#include <ir/parser/type_descriptor.h>
namespace luisa::ir {
class Parser {
	vstd::string errorMsg;
	vstd::unique_ptr<Kernel> kernel;
	vstd::HashMap<vstd::string, Type const*> customTypes;
	StatementName stateName;
	Type const* FindType(vstd::string_view name);
	vstd::vector<Var const*> functionVars;
	CommandRecorder recorder;
	uint64_t varIndex = 0;
	bool ParseStatement(
		char const*& ite,
		vstd::string_view& returnName,
		vstd::string_view& returnTypeName,
		vstd::string_view& stateName,
		vstd::vector<vstd::string_view>& argNames);
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
	uint64_t GetVarIndex() {
		return varIndex++;
	}
};
}// namespace luisa::ir