
#include <ShaderVariantCull/SVCull/PassType.h>
#include <Common/DynamicDLL.h>
#include <Database/DatabaseInclude.h>
using namespace toolhub::db;
//Implemented in SVCullTool.cpp
Database* GetDataBase();
vstd::string GenString(wchar_t const* p, uint ps);
class SVExportTool {
public:
	vstd::unique_ptr<IJsonDatabase> db;
	IJsonDict* varDict;
	vstd::function<WriteJsonVariant()> createDict;
	SVExportTool()
		: db(GetDataBase()->CreateDatabase()) {
		createDict = [&] {
			return db->CreateDict();
		};
	}
	void Regist(
		vstd::string&& shaderName,
		vstd::string&& shaderTypeName,
		vstd::string&& passName) {
		auto root = db->GetRootNode();

		auto shaderDict =
			root->TrySet(
					(shaderName),
					createDict)
				.template get_or<IJsonDict*>(nullptr);
		auto passDict =
			shaderDict->TrySet(
						  (passName),
						  createDict)
				.template get_or<IJsonDict*>(nullptr);
		varDict =
			passDict->TrySet(
						(shaderTypeName),
						createDict)
				.template get_or<IJsonDict*>(nullptr);
	}
	void Incre(vstd::string&& var) {
		varDict->TryReplace(
			(var),
			[&](ReadJsonVariant const& last) -> WriteJsonVariant {
				auto value = last.template get_or<int64>(0);
				return value + 1;
			});
	}
};

static SVExportTool set;
VENGINE_UNITY_EXTERN void SVExport_RegistExport(
	wchar_t const* shaderName,
	uint shaderNameSize,
	uint passType,
	uint shaderType) {
	set.Regist(
		GenString(shaderName, shaderNameSize),
		vstd::string(ShaderTypes[shaderType]),
		vstd::string(PassTypes[passType]));
}

VENGINE_UNITY_EXTERN void SVExport_IncreCount(
	wchar_t const* var,
	uint varSize) {
	set.Incre(GenString(var, varSize));
}

VENGINE_UNITY_EXTERN void SVExport_Print(
	wchar_t const* path,
	uint pathSize) {
	auto str = GenString(path, pathSize);
	auto result = set.db->GetRootNode()->FormattedPrint();
	auto f = fopen(str.c_str(), "wb");
	if (f) {
		auto disp = vstd::create_disposer([&] { fclose(f); });
		fwrite(result.data(), result.size(), 1, f);
	}
}