#pragma once
#include <Common/Common.h>
#include <Common/functional.h>
#include <Utility/VGuid.h>
namespace toolhub::db {
class IJsonDict;
class IJsonArray;
class Database;
class IJsonDatabase : public vstd::ISelfPtr {
public:
	virtual ~IJsonDatabase() = default;
	virtual IJsonDict* GetRootNode() = 0;
	virtual vstd::unique_ptr<IJsonDict> CreateDict() = 0;
	virtual vstd::vector<vstd::unique_ptr<IJsonDict>> CreateDicts(size_t count) = 0;
	virtual vstd::unique_ptr<IJsonArray> CreateArray() = 0;
	virtual vstd::vector<vstd::unique_ptr<IJsonArray>> CreateArrays(size_t count) = 0;
	virtual IJsonDict* CreateDict_RawPtr() = 0;
	virtual vstd::vector<IJsonDict*> CreateDicts_RawPtr(size_t count) = 0;
	virtual IJsonArray* CreateArray_RawPtr() = 0;
	virtual vstd::vector<IJsonArray*> CreateArrays_RawPtr(size_t count) = 0;
	////////// Extension
	virtual bool CompileFromPython(char const* code) {
		//Not Implemented
		return false;
	}
};

}// namespace toolhub::db