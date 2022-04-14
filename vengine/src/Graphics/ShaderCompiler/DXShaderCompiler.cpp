
#include <VEngineConfig.h>
#include <Graphics/IGraphicsFactory.h>
#ifdef _WIN32
#include <Graphics/ShaderCompiler/ShaderCompiler.h>
#include <Windows.h>
#include <dxc/dxcapi.h>
#include <Graphics/d3dx12.h>
#include <Graphics/ShaderCompiler/CodeSample/RegisterProcessor.h>
#include <Utility/Path.h>
using namespace Microsoft::WRL;
namespace toolhub::graphics {
template<typename T>
static vstd::wstring GetWStr(T const& str) {
	vstd::wstring ws;
	ws.resize(str.size());
	for (auto i : vstd::range(str.size())) {
		ws[i] = str[i];
	}
	return ws;
}
class DXByteBlob final : public IByteBlob {
public:
	vstd::ComUniquePtr<IDxcBlob> blob;
	vstd::ComUniquePtr<IDxcResult> comRes;
	DXByteBlob(
		decltype(blob)&& b,
		decltype(comRes)&& rr)
		: blob(std::move(b)),
		  comRes(std::move(rr)) {}
	vbyte* GetBufferPtr() const override {
		return reinterpret_cast<vbyte*>(blob->GetBufferPointer());
	}
	size_t GetBufferSize() const override {
		return blob->GetBufferSize();
	}
	VSTD_SELF_PTR
};
class DXShaderIncludeHandler : public IDxcIncludeHandler {
	vstd::HashMap<Path, void> includedFiles;
	ComPtr<IDxcIncludeHandler> pDefaultIncludeHandler;
	ComPtr<IDxcUtils> pUtils;
	void InitUtil() {
		ThrowIfFailed(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(pUtils.GetAddressOf())));
		ThrowIfFailed(pUtils->CreateDefaultIncludeHandler(pDefaultIncludeHandler.GetAddressOf()));
	}

public:
	vstd::string const* srcFilePath = nullptr;
	vstd::string GetString(LPCWSTR charPtr) {
		if constexpr (sizeof(std::remove_pointer_t<LPCWSTR>) == sizeof(wchar_t)) {
			auto ptr = reinterpret_cast<wchar_t const*>(charPtr);
			auto len = vstd::CharStrlen(ptr);
			vstd::string str;
			str.resize(len);
			for (auto i : vstd::range(len)) {
				str[i] = ptr[i];
			}
			return str;
		} else {
			return vstd::string(reinterpret_cast<char const*>(charPtr));
		}
	}
	void Clear() {
		includedFiles.Clear();
	}
	DXShaderIncludeHandler()
		: includedFiles(32) {
		InitUtil();
	}
	HRESULT STDMETHODCALLTYPE LoadSource(
		_In_ LPCWSTR pFilename,
		_COM_Outptr_result_maybenull_ IDxcBlob** ppIncludeSource) override {
		auto str = GetString(pFilename);
		if (str.size() < 4 || vstd::string_view(str.data(), str.data() + 4) != R"(.///)"_sv) {
			*ppIncludeSource = nullptr;
			return S_OK;
		}
		vstd::string_view realStr(str.data() + 4, str.end());
		// TODO
		Path folderPath = [&] {
			if (this->srcFilePath) {
				return Path(*this->srcFilePath).GetParentLevelPath();
			} else {
				return Path::GetProgramPath();
			}
		}();
		auto path = folderPath.GetSubLevelPath(vstd::string(realStr));
		if (includedFiles.TryEmplace(std::move(path)).second) {
			ComPtr<IDxcBlobEncoding> pEncoding;
			HRESULT hr = pUtils->LoadFile(GetWStr(path.GetPathStr()).c_str(), nullptr, pEncoding.GetAddressOf());
			if (SUCCEEDED(hr)) {
				*ppIncludeSource = pEncoding.Detach();
			} else {
				*ppIncludeSource = nullptr;
			}
		} else {
			*ppIncludeSource = nullptr;
		}
		return S_OK;
	}
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, _COM_Outptr_ void __RPC_FAR* __RPC_FAR* ppvObject) override {
		return pDefaultIncludeHandler->QueryInterface(riid, ppvObject);
	}

	ULONG STDMETHODCALLTYPE AddRef(void) override { return 0; }
	ULONG STDMETHODCALLTYPE Release(void) override { return 0; }
};
class DXShaderCompiler : public IShaderCompiler {
private:
	vstd::ComUniquePtr<IDxcCompiler3> comp;
	DXShaderIncludeHandler includeHandler;
	vstd::vector<vstd::wstring> preDefines;

public:
	vstd::wstring GetSM(uint shaderModel) {
		vstd::wstring smStr;
		smStr << vstd::to_wstring(shaderModel / 10) << L'_' << vstd::to_wstring(shaderModel % 10);
		return smStr;
	}
	DXShaderCompiler()
		: comp(vstd::create_comptr<IDxcCompiler3>([](auto ptr) {
			  return DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(ptr));
		  })) {
	}
	CompileResult Compile(
		vstd::string_view code,
		vstd::span<LPCWSTR> args,
		vstd::string const* srcFilePath) {
		DxcBuffer buffer{
			code.data(),
			code.size(),
			CP_UTF8};
		includeHandler.Clear();
		includeHandler.srcFilePath = srcFilePath;
		auto compileResult = vstd::create_comptr<IDxcResult>([&](auto ptr) {
			return comp->Compile(
				&buffer,
				args.data(),
				args.size(),
				&includeHandler,
				IID_PPV_ARGS(ptr));
		});
		HRESULT status;
		ThrowIfFailed(compileResult->GetStatus(&status));
		if (status == 0) {
			auto resultBlob = vstd::create_comptr<IDxcBlob>([&](auto ptr) {
				return compileResult->GetResult(ptr);
			});
			return vstd::unique_ptr<IByteBlob>(new DXByteBlob(std::move(resultBlob), std::move(compileResult)));
		} else {
			auto errBuffer = vstd::create_comptr<IDxcBlobEncoding>([&](auto ptr) {
				return compileResult->GetErrorBuffer(ptr);
			});
			auto errStr = vstd::string_view(
				reinterpret_cast<char const*>(errBuffer->GetBufferPointer()),
				errBuffer->GetBufferSize());
			return vstd::string(errStr);
		}
	}
	void AddCompileArgs(
		vstd::vector<LPCWSTR, VEngine_AllocType::VEngine, 32>& args,
		vstd::wstring const& smStr,
		bool optimize,
		vstd::span<vstd::string_view> defines) {
		if (!defines.empty()) {
			preDefines.clear();
			preDefines.reserve(defines.size());
			for (auto&& i : defines) {
				args.push_back(L"/D");
				auto&& s = preDefines.emplace_back(GetWStr(i));
				args.push_back(s.c_str());
			}
		}
		args.push_back(L"/T");
		args.push_back(smStr.c_str());
		args.push_back_all(
			{L"-Qstrip_debug",
			 L"-Qstrip_reflect",
			 L"/enable_unbounded_descriptor_tables",
			 L"-HV 2021",
			 L"/all_resources_bound"});
		if (optimize) {
			args.push_back(L"/O3");
		}
	}
	CompileResult CompileCompute(
		vstd::string_view code,
		bool optimize,
		uint shaderModel,
		vstd::string const* srcFilePath,
		vstd::span<vstd::string_view> defines) override {
		if (shaderModel < 10) {
			return "Illegal shader model!"_sv;
		}
		vstd::vector<LPCWSTR, VEngine_AllocType::VEngine, 32> args;
		vstd::wstring smStr;
		smStr << L"cs_" << GetSM(shaderModel);
		AddCompileArgs(args, smStr, optimize, defines);
		return Compile(code, args, srcFilePath);
	}
	CompileResult CompileRayTracing(
		vstd::string_view code,
		bool optimize,
		uint shaderModel,
		vstd::string const* srcFilePath,
		vstd::span<vstd::string_view> defines) override {
		if (shaderModel < 10) {
			return "Illegal shader model!"_sv;
		}
		vstd::vector<LPCWSTR, VEngine_AllocType::VEngine, 32> args;
		vstd::wstring smStr;
		smStr << L"lib_" << GetSM(shaderModel);
		AddCompileArgs(args, smStr, optimize, defines);
		return Compile(code, args, srcFilePath);
	}
	PreProcessResult PreProcessHLSL(vstd::string code) override {
		RegisterProcessor processor(std::move(code));
		processor.GenerateCode();
		return {
			std::move(processor.registerResults),
			std::move(processor.finalCode)};
	}
	static size_t FindFirstOf(vstd::string_view code, vstd::string_view s) {
		if (code.size() < s.size()) return vstd::string::npos;
		for (auto&& i : vstd::ptr_range(code.data(), code.size() - s.size())) {
			if (memcmp(&i, s.data(), s.size()) == 0)
				return (&i - code.data());
		}
		return vstd::string::npos;
	}
	vstd::unique_ptr<db::IJsonDict> GetHLSLInlineProperty(vstd::string_view code, db::IJsonDatabase* db) override {
		auto prop = "/*PROPERTY"_sv;
		auto sz = FindFirstOf(code, prop);
		if (sz == std::string::npos) {
			return nullptr;
		}
		sz += prop.size();
		auto nextCode = vstd::string_view(code.data() + sz, code.size() - sz);
		auto endSz = FindFirstOf(nextCode, "*/"_sv);
		if (endSz == std::string::npos) {
			return nullptr;
		}
		auto jsonCode = vstd::string_view(nextCode.data(), endSz);
		auto dd = db->CreateDict();
		auto error = dd->Parse(jsonCode, false);
		if (error) {
			std::cout << error->message << '\n';
			return nullptr;
		}
		return dd;
	}
	VSTD_SELF_PTR
};

IShaderCompiler* GraphicsFactory_Impl::CreateDirectXCompiler() const {
	return new DXShaderCompiler();
}

}// namespace toolhub::graphics

#else
namespace toolhub::graphics {
IShaderCompiler* GraphicsFactory_Impl::CreateDirectXCompiler() const {
	return nullptr;
}
}// namespace toolhub::graphics
#endif