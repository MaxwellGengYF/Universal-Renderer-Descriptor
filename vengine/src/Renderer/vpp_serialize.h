#pragma once
#include <Common/Common.h>
#include <Utility/VGuid.h>
#include <Unity/func_table.h>
#include <Utility/TaskThread.h>
namespace toolhub::vpp {
struct PathSpan {
	char const* str;
	size_t strLen;
};
struct SaveMeshArg {
	vstd::Guid fileUid;
	void* data;
	size_t byteSize;
	bool saveResult;
};
struct ReadMeshArg {
	vstd::Guid uid;
	void* ptr;
	size_t size;
};
struct ReadBatchedMeshArg {
	vstd::Guid uid;
	size_t offset;
	size_t size;
};
struct CullMeshArg {
	vstd::Guid* guid;
	size_t guidCount;
};
struct ReadDataArg {
	void* ptr;
	size_t size;
};
enum class FileLoadState : uint {
	UnFinished,
	Failed,
	Success
};
class VTable {
public:
	static unity::CallbackFuncPtr GetSingleMeshPath;
	static unity::CallbackFuncPtr GetBatchMeshFilePath;
	static unity::CallbackFuncPtr GetBatchFileName;
	static unity::CallbackFuncPtr UnityLog;
	static void Init();

	static void OnEnable(void*);
	static void GetNewUID(void* ptr);
	// SetMeshArg
	static void SaveMesh(void* saveMeshArg);
	// uint64
	static void RemoveMeshFile(void* uid);
	static void ReadMeshFile(void* readMeshFile);
	static void RemoveMeshFiles(void*);
	static void CullMeshFile(void*);
};
class Runtime {
public:
	std::atomic_bool success = false;
	TaskThread thread;
	vstd::HashMap<vstd::Guid, std::pair<size_t, size_t>> meshes;
	vstd::vector<std::byte> colorBuffer;
	Runtime();
	void operator()();
	void Clear();
	void BatchMesh(void*);
	void ReadMesh(void* readMesh);
	void SerBatchFile(void*);
	void DeserBatchFile(void*);
	void CompleteDeserBatchFile(void*);
	void GetDeserResult(void*);
	void GetVPPMemoryPtr(void* arg);
	void DeleteBatchFile(void*);
};
}// namespace toolhub::vpp