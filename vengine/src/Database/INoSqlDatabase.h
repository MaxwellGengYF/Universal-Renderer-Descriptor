#pragma once
#include <Database/IJsonObject.h>
namespace toolhub::db {
enum class CompareFlag : vbyte {
	Less,
	LessEqual,
	Equal,
	GreaterEqual,
	Greater,
	Always
};
enum class Operator : vbyte {
	And,
	Or
};
class Database;
class INoSqlDatabase : public vstd::ISelfPtr {
public:
	virtual ~INoSqlDatabase() = default;
	using KeyType = vstd::variant<
		int64,
		vstd::string,
		vstd::Guid>;
	using ValueType = vstd::variant<
		int64,
		double,
		vstd::string,
		vstd::Guid,
		bool>;
	using KeyValue = std::pair<KeyType, ValueType>;
	using CompareKey = std::tuple<KeyType, ValueType, CompareFlag>;
	using Table = vstd::HashMap<KeyType, ValueType>;
	using Tables = vstd::vector<Table, VEngine_AllocType::VEngine, 1>;
	struct Condition : public vstd::IOperatorNewBase {
		vstd::vector<
			vstd::variant<
				vstd::unique_ptr<Condition>,
				CompareKey>>
			keys;
		Operator op;
		Condition(Operator op) : op(op) {}
		Condition(Condition const&) = delete;
		Condition(Condition&&) = delete;
	};
	virtual void AddNode(vstd::IEnumerable<KeyValue>* keyValues) = 0;
	virtual void AddNode(IJsonDict* dictNode) = 0;
	virtual Tables FindAll(CompareKey const& key) = 0;
	virtual Tables FindAll(Condition const& key) = 0;
	virtual Tables FindAllAndDelete(CompareKey const& key) = 0;
	virtual Tables FindAllAndDelete(Condition const& key) = 0;
	virtual void DeleteAll(CompareKey const& key) = 0;
	virtual void DeleteAll(Condition const& key) = 0;
	virtual void Clear() = 0;
	virtual Tables GetAll() = 0;
};
}// namespace toolhub::db