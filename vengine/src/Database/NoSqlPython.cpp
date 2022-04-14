
#include <VEngineConfig.h>

#ifdef XX_VENGINE_PYTHON_SUPPORT
#include <Database/INoSqlDatabase.h>
#include <Database/DatabaseInclude.h>
#include <Utility/CommonIterators.h>
#include <Database/IJsonObject.h>
#include <Utility/StringUtility.h>
namespace toolhub::db::py {
VENGINE_UNITY_EXTERN Database const* Database_GetFactory();

class SearchResultHandle : public vstd::IOperatorNewBase {
public:
	using ExternMap = INoSqlDatabase::Table;
	vstd::vector<ExternMap> results;
	ExternMap* mapPtr;
	ExternMap::Iterator idx = nullptr;
};

static vstd::vector<INoSqlDatabase::KeyType> keyStack;
static vstd::vector<INoSqlDatabase::ValueType> valueStack;
static vstd::vector<CompareFlag> compFlagStack;
VENGINE_UNITY_EXTERN INoSqlDatabase* pynosql_create_db(char const* name, uint64 nameSize) {
	auto ptr = Database_GetFactory()->CreateSimpleNoSql(vstd::string_view(name, nameSize));
	return ptr;
}
VENGINE_UNITY_EXTERN void pynosql_dispose_handle(SearchResultHandle* handle) {
	delete handle;
}
VENGINE_UNITY_EXTERN void pynosql_dispose_db(INoSqlDatabase* ptr) {
	ptr->Dispose();
}
VENGINE_UNITY_EXTERN void pynosql_clear_db(INoSqlDatabase* ptr) {
	ptr->Clear();
}
VENGINE_UNITY_EXTERN void pynosql_print(INoSqlDatabase* ptr) {
	auto tables = ptr->GetAll();
	std::cout << "[\n";
	for (auto& i : tables) {
		std::cout << "{\n";
		for (auto& kv : i) {
			kv.first.visit([](auto&& v) { std::cout << v; });
			std::cout << ": ";
			kv.second.multi_visit(
				[](auto&& v) { std::cout << v; },
				[](auto&& v) { std::cout << v; },
				[](auto&& v) { std::cout << '"' << v << '"'; },
				[](auto&& v) { std::cout << v.ToString(); },
				[](auto&& v) {
					std::cout << (v ? "True" : "False");
				},
				[](auto&&) { std::cout << "null"; });
			std::cout << '\n';
		}
		std::cout << "}\n";
	}
	std::cout << "]\n";
}
// Search key , set value to valuePtr
/*
VENGINE_UNITY_EXTERN uint64 pynosql_get_key_size(SearchResultHandle* handle) {
	return handle->idx->first.size();
}
VENGINE_UNITY_EXTERN void pynosql_get_key(SearchResultHandle* handle, char* chr) {
	memcpy(chr, handle->idx->first.data(), handle->idx->first.size());
}*/
VENGINE_UNITY_EXTERN vbyte pynosql_get_key_type(SearchResultHandle* handle) {
	return handle->idx->first.GetType();
}
VENGINE_UNITY_EXTERN int64 pynosql_get_int_key(SearchResultHandle* handle) {
	return handle->idx->first.multi_visit_or(
		int64(0),
		[](auto&& v) { return v; },
		[](auto&& v) { return v.size(); },
		[](auto&& v) { return 0; });
}
VENGINE_UNITY_EXTERN void pynosql_get_str_key(SearchResultHandle* handle, char* ptr) {
	auto&& str = handle->idx->first. force_get<vstd::string>();
	memcpy(ptr, str.data(), str.size());
}
VENGINE_UNITY_EXTERN void pynosql_get_guid_key(SearchResultHandle* handle, vstd::Guid::GuidData* ptr) {
	*ptr = handle->idx->first. force_get<vstd::Guid>().ToBinary();
}
VENGINE_UNITY_EXTERN vbyte pynosql_get_value_type(SearchResultHandle* handle) {
	return handle->idx->second.GetType();
}
VENGINE_UNITY_EXTERN int64 pynosql_get_int_value(SearchResultHandle* handle) {
	return handle->idx->second.multi_visit_or(
		int64(0),
		[](auto&& it) { return it; },
		[](auto&& it) { return it; },
		[](auto&& it) { return it.size(); },
		[](auto&&) { return 0; },
		[](auto&& it) { return it; },
		[](auto&&) { return 0; });
}
VENGINE_UNITY_EXTERN double pynosql_get_double_value(SearchResultHandle* handle) {
	return handle->idx->second.multi_visit_or(
		double(0),
		[](auto&& it) { return it; },
		[](auto&& it) { return it; },
		[](auto&& it) { return it.size(); },
		[](auto&&) { return 0; },
		[](auto&& it) { return it; },
		[](auto&&) { return 0; });
}
VENGINE_UNITY_EXTERN void pynosql_get_str(SearchResultHandle* handle, char* ptr) {
	auto&& str = handle->idx->second. force_get<vstd::string>();
	memcpy(ptr, str.data(), str.size());
}
VENGINE_UNITY_EXTERN void pynosql_get_guid(SearchResultHandle* handle, vstd::Guid::GuidData* ptr) {
	*ptr = handle->idx->second. force_get<vstd::Guid>().ToBinary();
}
VENGINE_UNITY_EXTERN void pynosql_begin_map(SearchResultHandle* handle) {
	handle->mapPtr = handle->results.begin();
}
VENGINE_UNITY_EXTERN void pynosql_next_map(SearchResultHandle* handle) {
	++handle->mapPtr;
}
VENGINE_UNITY_EXTERN bool pynosql_map_end(SearchResultHandle* handle) {
	return handle->mapPtr == handle->results.end();
}

VENGINE_UNITY_EXTERN void pynosql_begin_ele(SearchResultHandle* handle) {
	handle->idx = handle->mapPtr->begin();
}
VENGINE_UNITY_EXTERN void pynosql_next_ele(SearchResultHandle* handle) {
	++handle->idx;
}
VENGINE_UNITY_EXTERN bool pynosql_ele_end(SearchResultHandle* handle) {
	return handle->idx == handle->mapPtr->end();
}

enum class SelectLogic : vbyte {
	Or,
	And
};
VENGINE_UNITY_EXTERN void pynosql_clear_condition() {
	keyStack.clear();
	valueStack.clear();
	compFlagStack.clear();
}
VENGINE_UNITY_EXTERN void pynosql_add_key_int(int64 value) {
	keyStack.emplace_back(value);
}
VENGINE_UNITY_EXTERN void pynosql_add_key_str(char const* ptr, uint64 sz) {
	keyStack.emplace_back(vstd::string_view(ptr, sz));
}
VENGINE_UNITY_EXTERN void pynosql_add_key_guid(vstd::Guid* guid) {
	keyStack.emplace_back(*guid);
}
VENGINE_UNITY_EXTERN void pynosql_add_value_int(int64 value) {
	valueStack.emplace_back(value);
}
VENGINE_UNITY_EXTERN void pynosql_add_value_double(double value) {
	valueStack.emplace_back(value);
}
VENGINE_UNITY_EXTERN void pynosql_add_value_str(char const* ptr, uint64 sz) {
	valueStack.emplace_back(vstd::string_view(ptr, sz));
}
VENGINE_UNITY_EXTERN void pynosql_add_value_bool(bool value) {
	valueStack.emplace_back(value);
}
VENGINE_UNITY_EXTERN void pynosql_add_value_null() {
	valueStack.emplace_back(nullptr);
}
VENGINE_UNITY_EXTERN void pynosql_add_value_guid(vstd::Guid* guid) {
	valueStack.emplace_back(*guid);
}
VENGINE_UNITY_EXTERN void pynosql_add_flag(CompareFlag flag) {
	compFlagStack.emplace_back(flag);
}
class CompareKeyValueIterator final : public vstd::IEnumerable<INoSqlDatabase::CompareKey> {
public:
	INoSqlDatabase::KeyType* keyPtr;
	INoSqlDatabase::ValueType* valuePtr;
	CompareFlag* flagPtr;
	CompareKeyValueIterator() {
		keyPtr = keyStack.data();
		valuePtr = valueStack.data();
		flagPtr = compFlagStack.data();
	}
	using T = INoSqlDatabase::CompareKey;
	T GetValue() override {
		return {std::move(*keyPtr), std::move(*valuePtr), std::move(*flagPtr)};
	}
	bool End() override {
		return keyPtr == keyStack.end() || valuePtr == valueStack.end() || flagPtr == compFlagStack.end();
	}
	void GetNext() override {
		keyPtr++;
		valuePtr++;
		flagPtr++;
	}
	vstd::optional<size_t> Length() { return keyStack.size(); }
	void Dispose() override { delete this; }
};
class KeyValueIterator final : public vstd::IEnumerable<INoSqlDatabase::KeyValue> {
public:
	INoSqlDatabase::KeyType* keyPtr;
	INoSqlDatabase::ValueType* valuePtr;
	KeyValueIterator() {
		keyPtr = keyStack.data();
		valuePtr = valueStack.data();
	}
	using T = INoSqlDatabase::KeyValue;
	T GetValue() override {
		return {std::move(*keyPtr), std::move(*valuePtr)};
	}
	bool End() override {
		return keyPtr == keyStack.end() || valuePtr == valueStack.end();
	}
	void GetNext() override {
		keyPtr++;
		valuePtr++;
	}
	vstd::optional<size_t> Length() { return keyStack.size(); }
	void Dispose() override { delete this; }
};

VENGINE_UNITY_EXTERN SearchResultHandle* pynosql_findone(INoSqlDatabase* db, bool deleteAfterFind, SelectLogic logic) {
	SearchResultHandle* handle = new SearchResultHandle();
	auto Add = [&](auto&& value) {
		if (value)
			handle->results.emplace_back(std::move(*value));
	};
	if (logic == SelectLogic::And) {
		if (deleteAfterFind)
			Add(db->FindOneAndDelete_And(vstd::get_rvalue_ptr(CompareKeyValueIterator())));
		else
			Add(db->FindOne_And(vstd::get_rvalue_ptr(CompareKeyValueIterator())));
	} else {
		if (deleteAfterFind)
			Add(db->FindOneAndDelete_Or(vstd::get_rvalue_ptr(CompareKeyValueIterator())));
		else
			Add(db->FindOne_Or(vstd::get_rvalue_ptr(CompareKeyValueIterator())));
	}
	return handle;
}
VENGINE_UNITY_EXTERN SearchResultHandle* pynosql_findall(INoSqlDatabase* db, bool deleteAfterFind, SelectLogic logic) {
	SearchResultHandle* handle = new SearchResultHandle();
	auto&& results = handle->results;
	if (logic == SelectLogic::And) {
		if (deleteAfterFind) {
			results = db->FindAllAndDelete_And(vstd::get_rvalue_ptr(CompareKeyValueIterator()));
		} else
			results = db->FindAll_And(vstd::get_rvalue_ptr(CompareKeyValueIterator()));
	} else {
		if (deleteAfterFind) {
			results = db->FindAllAndDelete_Or(vstd::get_rvalue_ptr(CompareKeyValueIterator()));
		} else
			results = db->FindAll_Or(vstd::get_rvalue_ptr(CompareKeyValueIterator()));
	}
	return handle;
}
VENGINE_UNITY_EXTERN void pynosql_deleteall(INoSqlDatabase* db, SelectLogic logic) {
	if (logic == SelectLogic::And) {
		db->DeleteAll_And(vstd::get_rvalue_ptr(CompareKeyValueIterator()));
	} else {
		db->DeleteAll_Or(vstd::get_rvalue_ptr(CompareKeyValueIterator()));
	}
}
VENGINE_UNITY_EXTERN void pynosql_addall(INoSqlDatabase* db) {
	db->AddNode(vstd::get_rvalue_ptr(KeyValueIterator()));
}
VENGINE_UNITY_EXTERN void pynosql_addall_json(INoSqlDatabase* db, IJsonDict* dct) {
	db->AddNode(dct);
}
}// namespace toolhub::db::py
#endif