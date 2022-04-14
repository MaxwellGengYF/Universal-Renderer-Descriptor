#pragma once
#include <DirectX/Shader/ShaderSerializer.h>
#include <Graphics/ShaderCompiler/ShaderCompiler.h>
#include <Utility/BinaryReader.h>
#include <Common/DynamicDLL.h>
#include <Database/DatabaseInclude.h>
#include <Graphics/IGraphicsFactory.h>
#include <Graphics/IByteBlob.h>
#include <Utility/StringUtility.h>
vstd::vector<std::pair<vstd::string, toolhub::directx::Shader::Property>> GetShaderProperty(
	vstd::vector<toolhub::graphics::IShader::Property>&& prep) {
	using namespace toolhub::directx;
	using namespace toolhub::graphics;
	vstd::vector<std::pair<vstd::string, Shader::Property>> result;
	uint registCount[] = {0, 0, 0};
	uint spaceCount[] = {0, 0, 0};
	for (auto&& i : prep) {
		Shader::Property prop;
		switch (i.type) {
			case IShader::PropertyType::CBuffer:
				prop.type = ShaderVariableType::ConstantBuffer;
				break;
			case IShader::PropertyType::Tex1D:
			case IShader::PropertyType::Tex2D:
			case IShader::PropertyType::Tex3D:
			case IShader::PropertyType::CubeMap:
				prop.type = ShaderVariableType::SRVDescriptorHeap;
				break;
			case IShader::PropertyType::RWTex1D:
			case IShader::PropertyType::RWTex2D:
			case IShader::PropertyType::RWTex3D:
				prop.type = ShaderVariableType::UAVDescriptorHeap;
				break;
			case IShader::PropertyType::Buffer:
			case IShader::PropertyType::Accel:
				if (i.arrSize) {
					prop.type = ShaderVariableType::SRVDescriptorHeap;
				} else {
					prop.type = ShaderVariableType::StructuredBuffer;
				}
				break;
			case IShader::PropertyType::RWBuffer:
				if (i.arrSize) {
					prop.type = ShaderVariableType::UAVDescriptorHeap;
				} else {
					prop.type = ShaderVariableType::RWStructuredBuffer;
				}
				break;
		}
		prop.spaceIndex = i.spaceIdx;
		prop.registerIndex = i.registIdx;
		prop.arrSize = i.arrSize;
		result.emplace_back(
			std::move(i.name),
			std::move(prop));
	}
	return result;
}
bool Compile() {
	using namespace std;
	using namespace toolhub;
	static DllFactoryLoader<db::Database> dbDll("VEngine_Database.dll", "Database_GetFactory");
	static DllFactoryLoader<graphics::IGraphicsFactory> graDll("VEngine_Graphics.dll", "Graphics_GetFactory");
	static vstd::unique_ptr<db::IJsonDatabase> db(dbDll()->CreateDatabase());
	static vstd::unique_ptr<graphics::IShaderCompiler> compiler(graDll()->CreateDirectXCompiler());
	vstd::string path = "D:/ToolHub/build/shaders/testRTX.compute";

	system("pause");
	if (path.size() == 4) {
		if (StringUtil::ToLower(vstd::string_view(path)) == "exit"_sv) return false;
	}
	if (path.empty()) return true;
	vstd::string_view fileName;
	for (auto ite = path.rbegin(); ite != path.rend(); ++ite) {
		auto&& v = *ite;
		if (v == '.') {
			fileName = vstd::string_view(&v, path.end());
		}
	}
	directx::Shader::Tag tag;
	if (fileName == ".rt"_sv) {
		tag = directx::Shader::Tag::RayTracingShader;
	} else if (fileName == ".compute"_sv) {
		tag = directx::Shader::Tag::ComputeShader;
	} else {
		std::cout << "Invalid file extension!\n";
		return true;
	}
	vstd::string outputPath(vstd::string_view(path.data(), fileName.data()));
	outputPath << ".cso";
	vstd::string code;
	{
		BinaryReader reader(path);
		code = reader.ReadToString();
	}
	auto dict = compiler->GetHLSLInlineProperty(code, db.get());
	//auto prep = compiler->PreProcessHLSL(std::move(code));
	auto compResult = [&] {
		if (tag == directx::Shader::Tag::RayTracingShader)
			return compiler->CompileRayTracing(code, true, 63, &path);
		return compiler->CompileCompute(code, true, 63, &path);
	}();

	auto&& err = compResult.template try_get<vstd::string>();
	if (err) {
		cout << "Compile Error: \n"
			 << *err << '\n';
		return true;
	}
	auto&& result = compResult.template get<0>();
	auto properties = GetShaderProperty({});
	auto serResult = [&] {
		bool a[] = {false, false, false};
		if (tag == directx::Shader::Tag::RayTracingShader) {
			a[0] = dict->Get("closest"_sv).get_or(false);
			a[1] = dict->Get("any"_sv).get_or(false);
			a[2] = dict->Get("intersect").get_or(false);
		}
		return directx::ShaderSerializer::Serialize(
			properties,
			{result->GetBufferPtr(), result->GetBufferSize()},
			tag,
			a[0], a[1], a[2]);
	}();
	auto f = fopen(outputPath.c_str(), "wb");
	cout << "Compile Success!"_sv;
	if (f) {
		auto disp = vstd::create_disposer([&] { fclose(f); });
		fwrite(serResult.data(), serResult.size(), 1, f);
	} else {
		cout << " But write failed!";
	}
	cout << '\n';
	return true;
}
int main() {
	std::cout << "Drag file's path!\n";
	while (Compile()) {}
	return 0;
}