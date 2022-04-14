
#include <Database/IJsonObject.h>
#include <Database/INoSqlDatabase.h>
#include <Database/SimpleBinaryJson.h>
#include <Database/DatabaseInclude.h>
#include <Common/TreeMap.h>
#include <Network/FunctionSerializer.h>
#include <shared_mutex>
#include <Utility/BinaryReader.h>
#include <Utility/StringUtility.h>
namespace toolhub::db {
class DictIterator final : public vstd::IEnumerable<INoSqlDatabase::KeyValue> {
public:
	using T = INoSqlDatabase::KeyValue;
	IJsonDict* dict;
	vstd::Iterator<JsonKeyPair> ite;
	DictIterator(
		IJsonDict* dict)
		: dict(dict),
		  ite(dict->begin()) {
	}
	T GetValue() override {
		auto value = *ite;
		auto guidToStr = [](vstd::Guid const& v) {
			vstd::string result;
			result.resize(25);
			result[0] = '$';
			v.ToBase64(result.data() + 1);
			result.resize(23);
			return result;
		};
		auto getKey = [&]() {
			return value.key.multi_visit_or(
				vstd::string(),
				[](auto&& v) { return vstd::to_string(v); },
				[](auto&& v) { return vstd::string(v); },
				guidToStr);
		};

		return [&] {
			if (!value.value.valid()) return T(getKey(), nullptr);
			return value.value.multi_visit_or(
				T(),
				[&](auto&& v) { return T(getKey(), v); },
				[&](auto&& v) { return T(getKey(), v); },
				[&](auto&& v) { return T(getKey(), v); },
				[&](auto&& v) { return T(getKey(), v->Print()); },
				[&](auto&& v) { return T(getKey(), v->Print()); },
				[&](auto&& v) { return T(getKey(), guidToStr(v)); },
				[&](auto&& v) { return T(getKey(), v); },
				[&](auto&&) { return T(getKey(), nullptr); });
		}();
	}
	bool End() override {
		return ite == vstd::IteEndTag();
	}
	void GetNext() override {
		++ite;
	}
	vstd::optional<size_t> Length() override { return dict->Length(); }
};

class NoSqlDatabase : public INoSqlDatabase {
private:
	struct Node;
	//SimpleBinaryJson db;
	using NodeVector = vstd::vector<std::pair<Node*, size_t>, VEngine_AllocType::VEngine, 4>;
	using KVMap = vstd::TreeMap<ValueType, NodeVector>;
	using GlobalMap = vstd::HashMap<KeyType, KVMap>;
	struct Node {
		vstd::vector<std::tuple<GlobalMap::Index, KVMap::Iterator, size_t>> vec;
	};

	GlobalMap globalMap;
	vstd::Pool<Node> pool;
	std::shared_mutex mtx;
	vstd::string name;
	Node* Allocate() {
		return pool.New();
	}
	void DeAllocate(Node* ptr) {
		auto RemoveNodeFromVec = [](KVMap& map, KVMap::Iterator idx, size_t tarIndex) {
			auto&& vec = idx->second;
			if (tarIndex < (vec.size() - 1)) {
				auto lastNode = vec.erase_last();
				std::get<2>(lastNode.first->vec[lastNode.second]) = tarIndex;
				vec[tarIndex] = lastNode;
			} else {
				vec.erase_last();
			}
			if (vec.empty()) {
				map.remove(idx);
			}
		};
		for (auto&& v : ptr->vec) {
			RemoveNodeFromVec(std::get<0>(v).Value(), std::get<1>(v), std::get<2>(v));
		}
		pool.Delete(ptr);
	}

	void GetNodes(CompareKey const& key, vstd::function<void(Node*)> const& func) {
		auto globalIte = globalMap.Find(std::get<0>(key));
		if (!globalIte)
			return;
		auto&& value = std::get<1>(key);
		auto&& kvMap = globalIte.Value();
		switch (std::get<2>(key)) {
			case CompareFlag::Equal: {
				if (!value.valid()) return;
				auto ite = kvMap.find(value);
				if (!ite)
					return;
				auto&& nodeVec = ite->second;

				for (auto&& i : nodeVec) {
					func(i.first);
				}
			}
				return;
			case CompareFlag::LessEqual: {
				if (!value.valid()) return;
				auto ite = kvMap.find_lequal(value);
				//start at next
				for (; ite; --ite) {
					auto&& nodeVec = ite->second;

					for (auto&& i : nodeVec) {
						func(i.first);
					}
				}
			}
				return;
			case CompareFlag::GreaterEqual: {
				if (!value.valid()) return;
				auto ite = kvMap.find_gequal(value);
				//start at next
				for (; ite; ++ite) {
					auto&& nodeVec = ite->second;

					for (auto&& i : nodeVec) {
						func(i.first);
					}
				}
			}
				return;
			case CompareFlag::Less: {
				if (!value.valid()) return;
				auto ite = kvMap.find_less(value);
				for (; ite; --ite) {
					auto&& nodeVec = ite->second;

					for (auto&& i : nodeVec) {
						func(i.first);
					}
				}
			}
				return;
			case CompareFlag::Greater: {
				if (!value.valid()) return;
				auto ite = kvMap.find_greater(value);
				for (; ite; ++ite) {
					auto&& nodeVec = ite->second;

					for (auto&& i : nodeVec) {
						func(i.first);
					}
				}
			}
				return;
			case CompareFlag::Always: {
				auto ite = kvMap.begin();
				for (; ite; ++ite) {
					auto&& nodeVec = ite->second;
					for (auto&& i : nodeVec) {
						func(i.first);
					}
				}
			}
				return;
		}

		return;
	}

	void ConditionSearch(Condition const& key, vstd::function<void(Node*)> const& func) {
		switch (key.keys.size()) {
			case 0: return;
			case 1: {
				auto&& k = key.keys[0];
				if (k.index() == 0) {
					ConditionSearch(*k.template get<0>(), func);
				} else if (k.index() == 1) {
					GetNodes(k.template get<1>(), func);
				}
			}
				return;
		}
		if (key.op == Operator::And) {
			vstd::HashMap<Node*, size_t> andMap;
			vstd::function<void(Node*)> getFunc = [&](Node* nd) -> void {
				auto ite = andMap.Emplace(nd, 0);
				ite.Value()++;
			};
			for (auto&& k : key.keys) {
				if (k.index() == 0) {
					ConditionSearch(*k.template get<0>(), getFunc);
				} else if (k.index() == 1) {
					GetNodes(k.template get<1>(), getFunc);
				}
			}
			for (auto&& i : andMap) {
				if (i.second == key.keys.size()) {
					func(i.first);
				}
			}
		} else {
			vstd::HashMap<Node*, void> orMap;
			vstd::function<void(Node*)> getFunc = [&](Node* nd) -> void {
				orMap.Emplace(nd);
			};
			for (auto&& k : key.keys) {
				if (k.index() == 0) {
					ConditionSearch(*k.template get<0>(), getFunc);
				} else if (k.index() == 1) {
					GetNodes(k.template get<1>(), getFunc);
				}
			}
			for (auto&& i : orMap) {
				func(i.first);
			}
		}
	}
	static Table GetNodeKV(Node* node) {
		Table vec(node->vec.size());
		for (auto&& vv : node->vec) {
			vec.Emplace(std::get<0>(vv).Key(),
						std::get<1>(vv)->first);
		}
		return vec;
	}
	template<bool deleteAfterFinish>
	vstd::optional<Table> GetVecFromNode(Node* nd) {
		if (!nd) return {};
		vstd::optional<Table> results;
		results.New(nd->vec.size());
		for (auto&& v : nd->vec) {
			results->Emplace(std::get<0>(v).Key(), std::get<1>(v)->first);
		}
		if constexpr (deleteAfterFinish) {
			DeAllocate(nd);
		}
		return results;
	}
	Table GetVecFromNonNullNode(Node* nd) {
		Table results(nd->vec.size());
		for (auto&& v : nd->vec) {
			results.Emplace(std::get<0>(v).Key(), std::get<1>(v)->first);
		}
		return results;
	}

public:
	NoSqlDatabase(vstd::string&& arg_name)
		: pool(64, true),
		  name(std::move(arg_name)) {
		if (!name.empty()) {
			BinaryReader reader(name);
			if (reader && reader.GetLength() > 0) {
				auto vec = reader.Read();
				vstd::span<vbyte const> sp = vec;
				auto kvCount = vstd::SerDe<uint64>::Get(sp);
				vstd::vector<KeyValue> allKeyValues;
				for (auto kv : vstd::range(kvCount)) {
					allKeyValues.clear();
					auto kvMapCount = vstd::SerDe<uint64>::Get(sp);
					allKeyValues.push_back_func(
						kvMapCount,
						[&]() {
							return KeyValue{vstd::SerDe<KeyType>::Get(sp),
											vstd::SerDe<ValueType>::Get(sp)};
						});
					mAddNode(allKeyValues);
					allKeyValues.clear();
				}
			}
		}
	}
	~NoSqlDatabase() {
		if (!name.empty()) {

			auto ite = pool.Iterator();
			FILE* f = fopen(name.c_str(), "wb");
			if (f) {
				vstd::vector<vbyte> vec;
				auto dispF = vstd::create_disposer([&]() {
					fclose(f);
				});
				vstd::SerDe<uint64>::Set(ite.size(), vec);
				for (auto&& i : ite) {
					vstd::SerDe<uint64>::Set(i->vec.size(), vec);
					for (auto&& v : i->vec) {
						auto&& key = std::get<0>(v).Key();
						auto&& value = std::get<1>(v)->first;
						vstd::SerDe<KeyType>::Set(key, vec);
						vstd::SerDe<ValueType>::Set(value, vec);
					}
					fwrite(vec.data(), vec.size(), 1, f);
					vec.clear();
				}
			}
		}
	}
	void* SelfPtr() override { return this; }
	void AddNode(vstd::IEnumerable<KeyValue>* keyValues) override {
		std::unique_lock lck(mtx);
		Node* nd = Allocate();
		for (; !keyValues->End(); keyValues->GetNext()) {
			auto kv = keyValues->GetValue();
			auto globalIte = globalMap.Emplace(std::move(kv.first));
			KVMap& map = globalIte.Value();
			auto ite = map.try_insert(std::move(kv.second)).first;
			auto&& vec = ite->second;
			size_t sz = vec.size();
			vec.emplace_back(nd, nd->vec.size());
			nd->vec.emplace_back(globalIte, ite, sz);
		}
	};
	void AddNode(IJsonDict* dictNode) override {
		AddNode(vstd::get_rvalue_ptr(DictIterator(dictNode)));
	}
	void mAddNode(vstd::vector<KeyValue> const& keyValues) {
		Node* nd = Allocate();
		for (auto&& kv : keyValues) {
			auto globalIte = globalMap.Emplace(std::move(kv.first));
			KVMap& map = globalIte.Value();
			auto ite = map.try_insert(std::move(kv.second)).first;
			auto&& vec = ite->second;
			size_t sz = vec.size();
			vec.emplace_back(nd, nd->vec.size());
			nd->vec.emplace_back(globalIte, ite, sz);
		}
	}
	Tables FindAll(CompareKey const& key) override {
		Tables result;
		vstd::function<void(Node*)> func = [&](Node* nd) -> void {
			result.emplace_back(GetVecFromNonNullNode(nd));
		};
		std::shared_lock lck(mtx);
		GetNodes(key, func);
		return result;
	}
	Tables FindAll(Condition const& key) override {
		Tables result;
		vstd::function<void(Node*)> func = [&](Node* nd) -> void {
			result.emplace_back(GetVecFromNonNullNode(nd));
		};
		std::shared_lock lck(mtx);
		ConditionSearch(key, func);
		return result;
	};

	Tables FindAllAndDelete(CompareKey const& key) override {
		Tables result;
		vstd::vector<Node*, VEngine_AllocType::VEngine, 8> needDeleteNode;
		vstd::function<void(Node*)> func = [&](Node* nd) -> void {
			result.emplace_back(GetVecFromNonNullNode(nd));
			needDeleteNode.emplace_back(nd);
		};
		std::unique_lock lck(mtx);
		GetNodes(key, func);
		for (auto i : needDeleteNode) {
			DeAllocate(i);
		}
		return result;
	}
	Tables FindAllAndDelete(Condition const& key) override {
		Tables result;
		vstd::vector<Node*, VEngine_AllocType::VEngine, 8> needDeleteNode;
		vstd::function<void(Node*)> func = [&](Node* nd) -> void {
			result.emplace_back(GetVecFromNonNullNode(nd));
			needDeleteNode.emplace_back(nd);
		};
		std::unique_lock lck(mtx);
		ConditionSearch(key, func);
		for (auto i : needDeleteNode) {
			DeAllocate(i);
		}
		return result;
	}

	void DeleteAll(CompareKey const& key) override {
		vstd::function<void(Node*)> func = [&](Node* nd) -> void {
			DeAllocate(nd);
		};
		std::unique_lock lck(mtx);
		GetNodes(key, func);
	}
	void DeleteAll(Condition const& key) override {
		vstd::function<void(Node*)> func = [&](Node* nd) -> void {
			DeAllocate(nd);
		};
		std::unique_lock lck(mtx);
		ConditionSearch(key, func);
	}

	void Clear() override {
		std::unique_lock lck(mtx);
		pool.DeleteAll();
		globalMap.Clear();
	}
	Tables GetAll() override {
		std::shared_lock lck(mtx);
		auto ite = pool.Iterator();
		Tables result;
		auto itePtr = ite.begin();
		result.push_back_func(
			ite.size(), [&]() {
				auto nds = itePtr;
				++itePtr;
				auto nd = *nds;
				return GetVecFromNonNullNode(nd);
			});
		return result;
	}
};

INoSqlDatabase* Database_Impl::CreateSimpleNoSql(vstd::string name) const {
	return new NoSqlDatabase(std::move(name));
}
INoSqlDatabase* Database_Impl::CreateSimpleNoSql(
	vstd::string name,
	vstd::function<void*(size_t)> const& allocFunc) const {
	return ((vstd::StackObject<NoSqlDatabase>*)allocFunc(sizeof(NoSqlDatabase)))->New(std::move(name));
}
vstd::unique_ptr<IJsonDict> Database_Impl::m_DBTableToDict(
	IJsonDatabase* db,
	INoSqlDatabase::Table const& table,
	vstd::vector<INoSqlDatabase::KeyValue>& illegal) const {
	auto stringToValue = [&](vstd::string_view str) -> WriteJsonVariant {
		if (str.size() > 0) {
			switch (str[0]) {
				case '{': {
					auto dct = db->CreateDict();
					auto err = dct->Parse(str, false);
					if (err) return str;
					return dct;
				}
				case '[': {
					auto arr = db->CreateArray();
					auto err = arr->Parse(str, false);
					if (err) return str;
					return arr;
				}
			}
		}
		return str;
	};
	vstd::unique_ptr<IJsonDict> ptr;
	ptr = db->CreateDict();
	ptr->Reserve(table.size());
	for (auto& kv : table) {
		auto key = kv.first.multi_visit_or(
			Key(),
			[](auto&& v) {
				return v;
			},
			[](auto&& v) -> vstd::string_view {
				return v;
			},
			[](auto&& v) {
				return v;
			});
		if (!kv.second.valid()) return nullptr;
		auto value = kv.second.multi_visit_or(
			WriteJsonVariant(),
			[](auto&& v) { return v; },
			[](auto&& v) { return v; },
			[&](auto&& v) {
				return stringToValue(v);
			},
			[](auto&& v) {
				return v;
			},
			[](auto&& v) { return v; });
		if (key.valid()) {
			ptr->Set(key, std::move(value));
		} else {
			illegal.emplace_back(kv.first, kv.second);
		}
	}
	return ptr;
}

std::pair<
	vstd::unique_ptr<IJsonDict>,
	vstd::vector<INoSqlDatabase::KeyValue>>
Database_Impl::DBTableToDict(
	IJsonDatabase* db,
	INoSqlDatabase::Table const& table) const {
	vstd::vector<INoSqlDatabase::KeyValue> vec;
	auto ptr = m_DBTableToDict(db, table, vec);
	return {std::move(ptr), std::move(vec)};
}

std::pair<
	vstd::unique_ptr<IJsonArray>,
	vstd::vector<INoSqlDatabase::KeyValue>>
Database_Impl::DBTablesToArray(
	IJsonDatabase* db,
	vstd::IEnumerable<INoSqlDatabase::Table>* tables) const {
	vstd::vector<INoSqlDatabase::KeyValue> vec;
	auto arr = db->CreateArray();
	auto capa = tables->Length();
	if (capa) {
		arr->Reserve(*capa);
	}
	for (; !tables->End(); tables->GetNext()) {
		auto tb = tables->GetValue();
		arr->Add(m_DBTableToDict(db, tb, vec));
	}
	return {std::move(arr), std::move(vec)};
}
}// namespace toolhub::db