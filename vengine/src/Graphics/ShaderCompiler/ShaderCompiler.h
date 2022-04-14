#pragma once
#include <Graphics/IByteBlob.h>
#include <Graphics/Struct.h>
#include <Graphics/SceneDescriptor/IShader.h>
namespace toolhub::graphics {
class IShaderCompiler : public vstd::ISelfPtr {
public:
	struct PreProcessResult {
		vstd::vector<IShader::Property> properties;
		vstd::string resultCode;
	};
	using CompileResult = vstd::variant<
		vstd::unique_ptr<IByteBlob>,
		vstd::string>;

	virtual CompileResult CompileCompute(
		vstd::string_view code,
		bool optimize,
		uint shaderModel = 63,
		vstd::string const* srcFilePath = nullptr,
		vstd::span<vstd::string_view> defines = {}) = 0;
	virtual CompileResult CompileRayTracing(
		vstd::string_view code,
		bool optimize,
		uint shaderModel = 63,
		vstd::string const* srcFilePath = nullptr,
		vstd::span<vstd::string_view> defines = {}) = 0;
	virtual PreProcessResult PreProcessHLSL(vstd::string code) = 0;
	virtual vstd::unique_ptr<db::IJsonDict> GetHLSLInlineProperty(vstd::string_view code, db::IJsonDatabase* db) = 0;
	virtual ~IShaderCompiler() = default;
};

}// namespace toolhub::graphics