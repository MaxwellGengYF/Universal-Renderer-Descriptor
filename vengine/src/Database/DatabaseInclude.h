#pragma once
#include <Common/Common.h>
#include <Database/INoSqlDatabase.h>
#include <Database/IJsonObject.h>
namespace toolhub::db {
class Database {
public:
	virtual IJsonDatabase* CreateDatabase() const = 0;
	virtual INoSqlDatabase* CreateSimpleNoSql(vstd::string name) const = 0;
	virtual INoSqlDatabase* CreateSimpleNoSql(
		vstd::string name,
		vstd::function<void*(size_t)> const& allocFunc) const = 0;
	virtual IJsonDatabase* CreateDatabase(
		vstd::function<void*(size_t)> const& allocFunc) const = 0;
	virtual std::pair<
		vstd::unique_ptr<IJsonDict>,
		vstd::vector<INoSqlDatabase::KeyValue>>
	DBTableToDict(
		IJsonDatabase* db,
		INoSqlDatabase::Table const& table) const = 0;
	virtual std::pair<
		vstd::unique_ptr<IJsonArray>,
		vstd::vector<INoSqlDatabase::KeyValue>>
	DBTablesToArray(
		IJsonDatabase* db,
		vstd::IEnumerable<INoSqlDatabase::Table>* tables) const = 0;
};
#ifdef VENGINE_DATABASE_PROJECT
VENGINE_UNITY_EXTERN Database const* Database_GetFactory();
class Database_Impl final : public Database {
public:
	IJsonDatabase* CreateDatabase() const override;
	IJsonDatabase* CreateDatabase(
		vstd::function<void*(size_t)> const& allocFunc) const override;
	INoSqlDatabase* CreateSimpleNoSql(vstd::string name) const override;
	vstd::unique_ptr<IJsonDict> m_DBTableToDict(
		IJsonDatabase* db,
		INoSqlDatabase::Table const& table,
		vstd::vector<INoSqlDatabase::KeyValue>& illegal) const;

	std::pair<
		vstd::unique_ptr<IJsonDict>,
		vstd::vector<INoSqlDatabase::KeyValue>>
	DBTableToDict(
		IJsonDatabase* db,
		INoSqlDatabase::Table const& table) const override;
	std::pair<
		vstd::unique_ptr<IJsonArray>,
		vstd::vector<INoSqlDatabase::KeyValue>>
	DBTablesToArray(
		IJsonDatabase* db,
		vstd::IEnumerable<INoSqlDatabase::Table>* tables) const override;
	INoSqlDatabase* CreateSimpleNoSql(
		vstd::string name,
		vstd::function<void*(size_t)> const& allocFunc) const override;
};
#endif
}// namespace toolhub::db