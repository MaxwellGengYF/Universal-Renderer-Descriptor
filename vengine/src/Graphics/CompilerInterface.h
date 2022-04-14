
#include <VEngineConfig.h>

enum class ShaderType : uint8_t {
	Compute,
	RayTracing
};
void* (*gMallocFunc)(size_t);
void (*gFreeFunc)(void*);
void* vengine_malloc(size_t size) {
	return gMallocFunc(size);
}
void vengine_free(void* ptr) {
	gFreeFunc(ptr);
}
VENGINE_UNITY_EXTERN void SetMemoryFunc(
	void* (*mallocFunc)(size_t),
	void (*freeFunc)(void*)) {
	gMallocFunc = mallocFunc;
	gFreeFunc = freeFunc;
}
VENGINE_UNITY_EXTERN void* GetCompiler();
VENGINE_UNITY_EXTERN bool CompileCompute(
	void* compiler,
	char const* code,
	size_t codeLen,
	uint shaderModel,
	ShaderType shaderType,
	bool optimize,
	void*& compileResult);
VENGINE_UNITY_EXTERN void* GetCompileResultBlob(
	void* result);
VENGINE_UNITY_EXTERN void* GetCompileResultError(
	void* result);
VENGINE_UNITY_EXTERN void GetBlobData(
	void* blob,
	void const*& data,
	size_t& dataSize);
VENGINE_UNITY_EXTERN void GetEncodingBlobData(
	void* blob,
	char const*& data,
	size_t& dataSize);
VENGINE_UNITY_EXTERN void DeleteResult(void* ptr);
VENGINE_UNITY_EXTERN void DeleteBlob(void* ptr);
VENGINE_UNITY_EXTERN void DeleteEncodingBlob(void* ptr);
VENGINE_UNITY_EXTERN void DeleteCompiler(void* ptr);

#include <Windows.h>
#include <dxc/dxcapi.h>
#include <string>
#include <comdef.h>
#include <vector>
#include <iostream>
class DxException {
public:
	DxException() = default;
	DxException(HRESULT hr, std::string functionName, std::wstring filename, int32_t lineNumber)
		: ErrorCode(hr),
		  FunctionName(std::move(functionName)),
		  Filename(std::move(filename)),
		  LineNumber(lineNumber) {}

	void ToString() const {
		// Get the string description of the error code.
		_com_error err(ErrorCode);
		std::wstring msg(err.ErrorMessage());

		std::cout << FunctionName << " failed in ";
		std::wcout << Filename;
		std::cout << "; line " << std::to_string(LineNumber) << "; error: ";
		std::wcout << msg;
	}
	HRESULT ErrorCode = S_OK;
	std::string FunctionName;
	std::wstring Filename;
	int32_t LineNumber = -1;
};
inline std::wstring AnsiToWString(const std::string& str) {
	WCHAR buffer[512];
	MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, buffer, 512);
	return std::wstring(buffer);
}
#ifndef ThrowIfFailed
#ifdef NDEBUG
#define ThrowIfFailed(x) (x)
#else
#define ThrowIfFailed(x)                                               \
	{                                                                  \
		HRESULT hr_ = (x);                                             \
		if (FAILED(hr_)) {                                             \
			std::wstring wfn = AnsiToWString(__FILE__);                \
			DxException(hr_, #x, std::move(wfn), __LINE__).ToString(); \
			std::cout << '\n';                                         \
		}                                                              \
	}
#endif
#endif
void* GetCompiler() {
	IDxcCompiler3* ptr = nullptr;
	ThrowIfFailed(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&ptr)));
	return ptr;
}
void DeleteCompiler(void* ptr) {
	reinterpret_cast<IDxcCompiler3*>(ptr)->Release();
}
void DeleteResult(void* ptr) {
	reinterpret_cast<IDxcResult*>(ptr)->Release();
}
std::wstring GetSM(uint shaderModel) {
	std::wstring smStr;
	smStr += std::to_wstring(shaderModel / 10);
	smStr += L'_';
	smStr += std::to_wstring(shaderModel % 10);
	return smStr;
}
bool CompileCompute(
	void* compilerVoid,
	char const* code,
	size_t codeLen,
	uint shaderModel,
	ShaderType shaderType,
	bool optimize,
	void*& compileResultVoid) {
	std::vector<LPCWSTR> args;
	std::wstring smStr;
	switch (shaderType) {
		case ShaderType::Compute:
			smStr += L"cs_";
			break;
		case ShaderType::RayTracing:
			smStr += L"lib_";
			break;
	}
	smStr += GetSM(shaderModel);
	args.push_back(L"/T");
	args.push_back(smStr.c_str());
	args.push_back(L"/enable_unbounded_descriptor_tables");
	args.push_back(L"/all_resources_bound");
	if (optimize) {
		args.push_back(L"/O3");
	}
	auto compiler = reinterpret_cast<IDxcCompiler3*>(compilerVoid);
	DxcBuffer buffer{
		code,
		codeLen,
		CP_UTF8};
	IDxcResult* compileResult = nullptr;
	auto result = compiler->Compile(
		&buffer,
		args.data(),
		args.size(),
		nullptr,
		IID_PPV_ARGS(&compileResult));
	HRESULT status;
	ThrowIfFailed(compileResult->GetStatus(&status));
	compileResultVoid = compileResult;
	return status == 0;
}
void* GetCompileResultBlob(
	void* resultVoid) {
	auto result = reinterpret_cast<IDxcResult*>(resultVoid);
	IDxcBlob* ptr = nullptr;
	ThrowIfFailed(result->GetResult(&ptr));
	return ptr;
}
void* GetCompileResultError(
	void* resultVoid) {
	auto result = reinterpret_cast<IDxcResult*>(resultVoid);
	IDxcBlobEncoding* ptr = nullptr;
	ThrowIfFailed(result->GetErrorBuffer(&ptr));
	return ptr;
}
void DeleteBlob(void* ptr) {
	reinterpret_cast<IDxcBlob*>(ptr)->Release();
}
void DeleteEncodingBlob(void* ptr) {
	reinterpret_cast<IDxcBlobEncoding*>(ptr)->Release();
}
void GetBlobData(
	void* blobVoid,
	void const*& data,
	size_t& dataSize) {
	auto blob = reinterpret_cast<IDxcBlob*>(blobVoid);
	data = blob->GetBufferPointer();
	dataSize = blob->GetBufferSize();
}
void GetEncodingBlobData(
	void* blobVoid,
	char const*& data,
	size_t& dataSize) {
	auto blob = reinterpret_cast<IDxcBlobEncoding*>(blobVoid);
	data = reinterpret_cast<char const*>(blob->GetBufferPointer());
	dataSize = blob->GetBufferSize();
}