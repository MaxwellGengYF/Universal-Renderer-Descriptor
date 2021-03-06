#pragma once
#include <Utility/VGuid.h>
#include <Database/IJsonDatabase.h>
#include <Database/ParserException.h>

namespace toolhub::db {
class IJsonDict;
class IJsonArray;

using ReadJsonVariant = vstd::variant<int64,
									  double,
									  vstd::string_view,
									  IJsonDict*,
									  IJsonArray*,
									  vstd::Guid,
									  bool,
									  std::nullptr_t>;
using WriteJsonVariant = vstd::variant<int64,
									   double,
									   vstd::string,
									   vstd::unique_ptr<IJsonDict>,
									   vstd::unique_ptr<IJsonArray>,
									   vstd::Guid,
									   bool>;
using Key = vstd::variant<
	int64,
	vstd::string_view,
	vstd::Guid>;
struct JsonKeyPair {
	Key key;
	ReadJsonVariant value;
	JsonKeyPair(
		Key&& key,
		ReadJsonVariant&& value) : key(std::move(key)), value(std::move(value)) {}
};
struct MoveJsonKeyPair {
	Key key;
	WriteJsonVariant value;
	MoveJsonKeyPair(
		Key&& key,
		WriteJsonVariant&& value) : key(std::move(key)), value(std::move(value)) {}
};
class IJsonObject : public vstd::IDisposable {
protected:
	virtual ~IJsonObject() = default;

public:
	////////// Basic
	virtual bool IsDict() const { return false; }
	virtual bool IsArray() const { return false; }
	virtual IJsonDatabase* GetDB() const = 0;
	virtual size_t Length() const = 0;
	virtual vstd::vector<vbyte> Serialize() const = 0;
	virtual void Serialize(vstd::vector<vbyte>& vec) const = 0;
	virtual void Reset() = 0;
	virtual bool IsEmpty() const = 0;
	virtual vstd::string FormattedPrint() const = 0;
	virtual vstd::string Print() const = 0;
	virtual bool Read(vstd::span<vbyte const> sp,
					  bool clearLast) = 0;
	virtual void Reserve(size_t capacity) = 0;
	virtual vstd::optional<ParsingException> Parse(
		vstd::string_view str,
		bool clearLast) = 0;
	virtual vstd::MD5 GetMD5() const = 0;
	vstd::IteEndTag end() const { return vstd::IteEndTag(); }
};

class IJsonDict : public IJsonObject {

protected:
	~IJsonDict() = default;

public:
	bool IsDict() const override { return true; }
	virtual ReadJsonVariant Get(Key const& key) const = 0;
	virtual vstd::vector<ReadJsonVariant> Get(vstd::span<Key> keys) const = 0;
	virtual bool Contains(Key const& key) const = 0;
	virtual vstd::vector<bool> Contains(vstd::span<Key> keys) const = 0;
	virtual void Set(Key const& key, WriteJsonVariant&& value) = 0;
	virtual void Set(vstd::span<std::pair<Key, WriteJsonVariant>> kv) = 0;
	virtual ReadJsonVariant TrySet(Key const& key, vstd::function<WriteJsonVariant()> const& value) = 0;
	virtual void TryReplace(Key const& key, vstd::function<WriteJsonVariant(ReadJsonVariant const&)> const& value) = 0;
	virtual void Remove(Key const& key) = 0;
	virtual vstd::optional<WriteJsonVariant> GetAndRemove(Key const& key) = 0;
	virtual vstd::optional<WriteJsonVariant> GetAndSet(Key const& key, WriteJsonVariant&& newValue) = 0;
	virtual void Remove(vstd::span<Key> keys) = 0;
	virtual vstd::vector<std::pair<Key, ReadJsonVariant>> ToVector() const = 0;
	virtual vstd::Iterator<JsonKeyPair> begin() const& = 0;
	virtual vstd::Iterator<MoveJsonKeyPair> begin() && = 0;
	virtual vstd::optional<ParsingException> ParseYaml(
		vstd::string_view str,
		bool clearLast) = 0;
	virtual vstd::string PrintYaml() const = 0;

	IJsonDict& operator<<(std::pair<Key, WriteJsonVariant>&& value) {
		Set(value.first, std::move(value.second));
		return *this;
	}
	ReadJsonVariant operator[](Key const& key) const {
		return Get(key);
	}
};

class IJsonArray : public IJsonObject {

protected:
	~IJsonArray() = default;

public:
	bool IsArray() const override { return true; }
	virtual ReadJsonVariant Get(size_t index) const = 0;
	virtual vstd::vector<ReadJsonVariant> Get(vstd::span<size_t> indices) const = 0;
	virtual void Set(size_t index, WriteJsonVariant&& value) = 0;
	virtual void Set(vstd::span<std::pair<size_t, WriteJsonVariant>> values) = 0;
	virtual void Remove(size_t index) = 0;
	virtual void Remove(vstd::span<size_t> indices) = 0;
	virtual vstd::optional<WriteJsonVariant> GetAndRemove(size_t index) = 0;
	virtual vstd::optional<WriteJsonVariant> GetAndSet(size_t key, WriteJsonVariant&& newValue) = 0;
	virtual void Add(WriteJsonVariant&& value) = 0;
	virtual void Add(vstd::span<WriteJsonVariant> values) = 0;
	virtual vstd::vector<ReadJsonVariant> ToVector() const = 0;
	virtual vstd::Iterator<ReadJsonVariant> begin() const& = 0;
	virtual vstd::Iterator<WriteJsonVariant> begin() && = 0;
	IJsonArray& operator<<(WriteJsonVariant&& value) {
		Add(std::move(value));
		return *this;
	}
	ReadJsonVariant operator[](size_t index) const {
		return Get(index);
	}
};
}// namespace toolhub::db