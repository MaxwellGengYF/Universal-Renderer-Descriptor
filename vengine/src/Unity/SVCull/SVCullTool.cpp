
#include <Unity/SVCull/PassType.h>
#include <Common/DynamicDLL.h>
#include <Database/DatabaseInclude.h>
#include <Utility/BinaryReader.h>
using namespace toolhub::db;
enum class RegistType : uint {
	None,
	DeleteAll,
	Continue
};
Database* GetDataBase() {
	static DllFactoryLoader<Database> fac("VEngine_Database.dll", "Database_GetFactory");
	return fac();
}
class SVCullTool {
public:
	vstd::unique_ptr<IJsonDatabase> db;
	IJsonDict* passDict = nullptr;
	vstd::vector<vstd::string> shaderVar;
	SVCullTool()
		: db(GetDataBase()->CreateDatabase())
	{
	}
	bool ParseFile(vstd::string const& path) const {
		BinaryReader reader(path);
		auto fileData = reader.ReadToString();
		auto parseResult = db->GetRootNode()->Parse(fileData, false);
		if (parseResult) {
			return false;
		}
		return true;
	}
	//Return: false: dont delete, true: delete
	RegistType RegistShaderPass(
		vstd::string const& shaderName,
		uint shaderPass) {

		auto shaderRead = db->GetRootNode()->Get(shaderName);
		if (!shaderRead.valid()) return RegistType::None;
		auto shaderDict = shaderRead.template get_or<IJsonDict*>(nullptr);
		if (!shaderDict) return RegistType::DeleteAll;
		auto passRead = shaderDict->Get(PassTypes[shaderPass]);
		if (!passRead.valid()) return RegistType::None;
		passDict = passRead.template get_or<IJsonDict*>(nullptr);
		if (!passDict) return RegistType::DeleteAll;
		return RegistType::Continue;
	}
	bool CullShaderVar() const {
		if (!passDict) return false;
		for (auto&& i : shaderVar) {
			auto result = passDict->Get(i);
			if (!result.valid()) continue;
			return result.get_or(true);
		}
		return false;
	}
};

static SVCullTool sct;

vstd::string GenString(wchar_t const* p, uint ps) {
	vstd::string result;
	result.resize(ps);
	for (auto i : vstd::range(ps)) {
		result[i] = p[i];
	}
	return result;
}
VENGINE_UNITY_EXTERN bool SVCull_Parse(wchar_t const* path, uint pathSize) {
	auto r = GenString(path, pathSize);
	return sct.ParseFile(r);
}
VENGINE_UNITY_EXTERN RegistType SVCull_RegistShaderPass(wchar_t const* shaderName, uint shaderNameSize, uint shaderPass) {
	auto r = GenString(shaderName, shaderNameSize);
	return sct.RegistShaderPass(r, shaderPass);
}
VENGINE_UNITY_EXTERN void SVCull_AddVar(wchar_t const* shaderName, uint shaderNameSize) {
	sct.shaderVar.push_back(GenString(shaderName, shaderNameSize));
}
VENGINE_UNITY_EXTERN bool SVCull_CullShaderVar() {
	return sct.CullShaderVar();
}
VENGINE_UNITY_EXTERN void SVCull_ClearList() {
	sct.shaderVar.clear();
}