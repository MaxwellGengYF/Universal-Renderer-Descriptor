#pragma once
#include <Common/Common.h>
#include <Database/SimpleJsonLoader.h>
#include <Utility/VGuid.h>
namespace toolhub::db {

class SimpleJsonLoader;
class SimpleJsonObject : public vstd::IDisposable{
protected:
	vstd::Guid selfGuid;
	SimpleBinaryJson* db = nullptr;
	SimpleJsonObject(
		vstd::Guid const& guid,
		SimpleBinaryJson* db);
	~SimpleJsonObject() {}

public:
	vstd::HashMap<vstd::Guid, std::pair<SimpleJsonObject*, vbyte>>::Index dbIndexer;
	SimpleBinaryJson* GetDB() const { return db; }
	vstd::Guid const& GetGUID() const { return selfGuid; }
	virtual void M_GetSerData(vstd::vector<vbyte>& data) = 0;
	virtual void LoadFromData(vstd::span<vbyte const> data) = 0;
	virtual void AfterAdd(IDatabaseEvtVisitor* visitor) = 0;
	virtual void BeforeRemove(IDatabaseEvtVisitor* visitor) = 0;
};
}// namespace toolhub::db