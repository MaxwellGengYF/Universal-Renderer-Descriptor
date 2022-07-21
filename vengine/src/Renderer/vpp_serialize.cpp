#include "vpp_serialize.h"
#include <Common/string_builder.h>
#include <Utility/FileUtility.h>
#include <filesystem>
#include <Utility/BinaryReader.h>
#include <Utility/StringUtility.h>
#include <Common/ranges.h>
#include <Common/tuple.h>
#include <Utility/file_range.h>
namespace toolhub::vpp {
#define SET_VPP_FUNCPTR(obj, name) table->SetCppFuncPtr(#name##_sv, [](void* arg) { obj.name(arg); })
vstd::unique_ptr<unity::FuncTable> table;
unity::CallbackFuncPtr VTable::GetSingleMeshPath;
unity::CallbackFuncPtr VTable::GetBatchMeshFilePath;
unity::CallbackFuncPtr VTable::GetBatchFileName;
unity::CallbackFuncPtr VTable::UnityLog;
static void Log(vstd::string const& str) {
	std::pair<char const*, size_t> arg(str.data(), str.size());
	VTable::UnityLog(&arg);
}
vstd::string singleFilePath;
vstd::string batchFilePath;
vstd::string batchFileName;
vstd::vector<std::byte> byteArr;
struct Header {
	size_t magicNumber;
	size_t byteSize;
};

struct BatchHeader {
	size_t magicNumber;
	size_t colorByteSize;
	size_t meshCount;
};

static constexpr size_t SingleMeshMagicNum = 247945482575826104ull;
static constexpr size_t BatchMeshMagicNum = 11915511972148456135ull;
static Runtime gRuntime;

static void SetFilePath(unity::CallbackFuncPtr getPath, vstd::string& filePath) {
	PathSpan path;
	getPath(&path);
	filePath = vstd::string_view(path.str, path.strLen);
	if (!filePath.empty()) {
		for (auto&& i : filePath) {
			if (i == '\\') i = '/';
		}
		if (*(filePath.end() - 1) != '/')
			filePath << '/';
	}
}
void VTable::GetNewUID(void* ptr) {
	*reinterpret_cast<vstd::Guid*>(ptr) = vstd::Guid(true);
}

void VTable::Init() {
	gRuntime.Clear();
	GET_CSHARP_FUNC_PTR(table, GetSingleMeshPath);
	GET_CSHARP_FUNC_PTR(table, GetBatchMeshFilePath);
	GET_CSHARP_FUNC_PTR(table, GetBatchFileName);
	GET_CSHARP_FUNC_PTR(table, UnityLog);
	SET_CPP_FUNC_PTR(table, SaveMesh);
	SET_CPP_FUNC_PTR(table, RemoveMeshFile);
	SET_CPP_FUNC_PTR(table, RemoveMeshFiles);
	SET_CPP_FUNC_PTR(table, GetNewUID);
	SET_CPP_FUNC_PTR(table, OnEnable);
	SET_CPP_FUNC_PTR(table, ReadMeshFile);
	SET_CPP_FUNC_PTR(table, CullMeshFile);
	SET_VPP_FUNCPTR(gRuntime, BatchMesh);
	SET_VPP_FUNCPTR(gRuntime, ReadMesh);
	SET_VPP_FUNCPTR(gRuntime, SerBatchFile);
	SET_VPP_FUNCPTR(gRuntime, DeserBatchFile);
	SET_VPP_FUNCPTR(gRuntime, GetVPPMemoryPtr);
	SET_VPP_FUNCPTR(gRuntime, DeleteBatchFile);
	SET_VPP_FUNCPTR(gRuntime, CompleteDeserBatchFile);
	SET_VPP_FUNCPTR(gRuntime, GetDeserResult);
}

void VTable::OnEnable(void*) {
	SetFilePath(GetSingleMeshPath, singleFilePath);
	SetFilePath(GetBatchMeshFilePath, batchFilePath);
	PathSpan path;
	GetBatchFileName(&path);
	batchFileName = vstd::string_view(path.str, path.strLen);
}
void VTable::SaveMesh(void* arg) {
	auto&& a = *reinterpret_cast<SaveMeshArg*>(arg);
	vstd::string filePath;
	std::filesystem::create_directory(singleFilePath.c_str());
	filePath << singleFilePath << a.fileUid.ToString(true) << ".vpbytes"_sv;
	auto f = fopen(filePath.c_str(), "wb");
	if (!f) {
		a.saveResult = false;
		return;
	}
	auto disp = vstd::create_disposer([&] { fclose(f); });
	Header header{
		.byteSize = a.byteSize,
		.magicNumber = SingleMeshMagicNum};
	fwrite(&header, sizeof(Header), 1, f);
	fwrite(a.data, a.byteSize, 1, f);
	a.saveResult = true;
}
void VTable::RemoveMeshFile(void* arg) {
	vstd::string filePath;
	vstd::StringBuilder(&filePath) << singleFilePath << reinterpret_cast<vstd::Guid*>(arg)->ToString(true) << ".vpbytes"_sv;
	std::error_code err;
	std::filesystem::remove(filePath.c_str(), err);
}
void VTable::ReadMeshFile(void* ptr) {
	auto& a = *reinterpret_cast<ReadMeshArg*>(ptr);
	vstd::string filePath;
	vstd::StringBuilder(&filePath) << singleFilePath << a.uid.ToString(true) << ".vpbytes"_sv;
	BinaryReader reader(filePath);
	auto ClearPtr = [&] {
		a.ptr = nullptr;
		a.size = 0;
	};

	if (!reader) {
		ClearPtr();
		return;
	}
	Header h;
	reader.Read(&h, sizeof(Header));
	if (h.magicNumber != SingleMeshMagicNum) {
		ClearPtr();
		return;
	}
	byteArr.clear();
	byteArr.resize(h.byteSize);
	a.size = h.byteSize;
	a.ptr = byteArr.data();
	reader.Read(a.ptr, a.size);
}
void VTable::RemoveMeshFiles(void*) {
	std::error_code err;
	std::filesystem::remove(singleFilePath.c_str(), err);
}
static vstd::optional<vstd::Guid> ParseGuidFromPath(vstd::string_view filePath) {
	char const* endPtr = filePath.data() + filePath.size();
	[&] {
		auto ptr = filePath.data() + filePath.size() - 1;
		for (; ptr != filePath.data() - 1; --ptr) {
			if (*ptr == '.') {
				endPtr = ptr;
				return;
			}
		}
		endPtr = ptr;
	}();
	char const* startPtr = filePath.data();
	for (auto ptr = endPtr; ptr != filePath.data() - 1; --ptr) {
		if (*ptr == '/' || *ptr == '\\') {
			startPtr = ptr + 1;
			break;
		}
	}
	vstd::string_view uidStr(startPtr, endPtr);
	auto num = vstd::Guid::TryParseGuid(uidStr);
	return num;
}
void VTable::CullMeshFile(void* aptr) {
	auto& a = *reinterpret_cast<CullMeshArg*>(aptr);
	vstd::HashMap<vstd::Guid> map;
	map.reserve(a.guidCount);
	for (auto&& i : vstd::ptr_range(a.guid, a.guidCount)) {
		map.Emplace(i);
	}
	auto DeleteFile = [&](vstd::string const& path) {
		std::error_code err;
		std::filesystem::remove(path.c_str(), err);
	};
	auto ite =
		vstd::FileRange(singleFilePath) |
		vstd::TransformRange(
			[&](vstd::string const& filePath) {
				auto num = ParseGuidFromPath(filePath);
				return std::pair<decltype(num), vstd::string>{num, std::move(filePath)};
			}) |
		vstd::FilterRange([&](auto&& guidPath) {
			return !(guidPath.first.has_value() && !map.Find(*guidPath.first));
		});
	for (auto&& i : ite) {
		DeleteFile(i.second);
	}
}
void Runtime::Clear() {
	meshes.Clear();
	colorBuffer.clear();
}
void Runtime::BatchMesh(void*) {
	meshes.Clear();
	colorBuffer.clear();
	for (auto const& filePath : vstd::FileRange(singleFilePath)) {
		auto num = ParseGuidFromPath(filePath);
		if (!num) {
			continue;
		}
		BinaryReader reader(filePath);
		Header header;
		reader.Read(&header, sizeof(Header));
		if (header.magicNumber != SingleMeshMagicNum) continue;
		auto lastSize = colorBuffer.size();
		colorBuffer.resize(lastSize + header.byteSize);
		auto ptr = colorBuffer.data() + lastSize;
		reader.Read(ptr, header.byteSize);
		meshes.ForceEmplace(*num, lastSize, header.byteSize);
	}
}
void Runtime::ReadMesh(void* readMesh) {
	auto& a = *reinterpret_cast<ReadBatchedMeshArg*>(readMesh);
	auto ite = meshes.Find(a.uid);
	if (!ite) {
		a.offset = 0;
		a.size = 0;
		return;
	}
	auto& v = ite.Value();
	a.offset = v.first;
	a.size = v.second;
}
void Runtime::SerBatchFile(void*) {
	vstd::string path;
	path << batchFilePath << batchFileName;
	std::filesystem::create_directory(batchFilePath.c_str());
	auto f = fopen(path.c_str(), "wb");
	if (!f) return;
	auto disp = vstd::create_disposer([&] {
		fclose(f);
	});
	BatchHeader header{
		.magicNumber = BatchMeshMagicNum,
		.colorByteSize = colorBuffer.byte_size(),
		.meshCount = meshes.size()};
	fwrite(&header, sizeof(header), 1, f);
	for (auto&& i : meshes) {
		fwrite(&i.first, sizeof(vstd::Guid), 1, f);
		fwrite(&i.second, sizeof(size_t) * 2, 1, f);
	}
	fwrite(colorBuffer.data(), colorBuffer.byte_size(), 1, f);
}
void Runtime::DeserBatchFile(void*) {
	meshes.Clear();
	colorBuffer.clear();
	success = false;
	thread.ExecuteNext();
}
void Runtime::DeleteBatchFile(void*) {
	meshes.Clear();
	colorBuffer.clear();
	std::error_code err;
	std::filesystem::remove(batchFilePath.c_str(), err);
}

void Runtime::GetVPPMemoryPtr(void* a) {
	auto& arg = *reinterpret_cast<ReadDataArg*>(a);
	arg.ptr = colorBuffer.data();
	arg.size = colorBuffer.size();
}
void Runtime::operator()() {
	vstd::string path;
	path << batchFilePath << batchFileName;
	BinaryReader reader(path);
	if (!reader) {
		success = false;
		return;
	}
	BatchHeader header;
	reader.Read(&header, sizeof(BatchHeader));
	if (header.magicNumber != BatchMeshMagicNum) {
		success = false;
		return;
	}
	meshes.Reserve(header.meshCount);
	for (auto i : vstd::range(header.meshCount)) {
		vstd::tuple<vstd::Guid::GuidData, size_t, size_t> kv;
		reader.Read(&kv, sizeof(kv));
		meshes.Emplace(vstd::Guid(kv.get<0>()), kv.get<1>(), kv.get<2>());
	}
	colorBuffer.resize(header.colorByteSize);
	reader.Read(colorBuffer.data(), colorBuffer.byte_size());
	success = true;
}
void Runtime::CompleteDeserBatchFile(void*) {
	thread.Complete();
}
void Runtime::GetDeserResult(void* value) {
	auto& a = *reinterpret_cast<FileLoadState*>(value);
	if (!thread.IsCompleted()) {
		a = FileLoadState::UnFinished;
	} else {
		a = success ? FileLoadState::Success : FileLoadState::Failed;
	}
}
Runtime::Runtime() {
	thread.SetFunctor(*this);
}
VENGINE_UNITY_EXTERN void vppInit(unity::FuncTable* funcTable) {
	table = vstd::create_unique(funcTable);
	VTable::Init();
}
}// namespace toolhub::vpp
#ifdef DEBUG

int main() {
	return 0;
}
#endif
#undef SET_VPP_FUNCPTR