#pragma once
#include <Common/Common.h>
#include <Network/FunctionSerializer.h>
#include <Utility/MD5.h>
namespace vstd {
class VENGINE_DLL_COMMON Guid {
public:
	struct GuidData {
		uint64 data0;
		uint64 data1;
	};

private:
	GuidData data;

public:
	friend VENGINE_DLL_COMMON std::ostream& operator<<(std::ostream& out, const Guid& obj) noexcept;

	explicit Guid(bool generate);
	Guid(vstd::string_view strv);
	static optional<Guid> TryParseGuid(vstd::string_view strv);
	Guid(vstd::span<vbyte> data);
	Guid(MD5 const& md5) {
		auto&& bin = md5.ToBinary();
		data.data0 = bin.data0;
		data.data1 = bin.data1;
	}
	operator MD5() const {
		return MD5(MD5::MD5Data{data.data0, data.data1});
	}
	Guid(std::array<vbyte, sizeof(GuidData)> const& data);
	Guid(GuidData const& d);
	void ReGenerate();
	Guid(Guid const&) = default;
	Guid(Guid&&) = default;
	void Reset() {
		data.data0 = 0;
		data.data1 = 0;
	}
	GuidData const& ToBinary() const { return data; }
	std::array<vbyte, sizeof(GuidData)> ToArray() const;
	vstd::string ToString(bool upper = true) const;
	void ToString(char* result, bool upper = true) const;
	vstd::string ToBase64() const;
	void ToBase64(char* result) const;

	inline bool operator==(Guid const& d) const {
		return data.data0 == d.data.data0 && data.data1 == d.data.data1;
	}
	inline bool operator!=(Guid const& d) const {
		return !operator==(d);
	}
	inline Guid& operator=(Guid const& d) {
		this->~Guid();
		new (this) Guid(d);
		return *this;
	}
	inline Guid& operator=(GuidData const& d) {
		this->~Guid();
		new (this) Guid(d);
		return *this;
	}
	inline operator bool() const {
		return data.data0 != 0 && data.data1 != 0;
	}
	inline bool operator!() const {
		return !(operator bool());
	}
};
template<>
struct hash<Guid> {
	size_t operator()(Guid const& guid) const {
		return Hash::CharArrayHash(
			&guid.ToBinary().data0,
			sizeof(Guid::GuidData));
	}
};
template<>
struct compare<Guid> {
	int32 operator()(Guid const& a, Guid const& b) const {
		if (a.ToBinary().data0 > b.ToBinary().data0) return 1;
		if (a.ToBinary().data0 < b.ToBinary().data0) return -1;
		if (a.ToBinary().data1 > b.ToBinary().data1) return 1;
		if (a.ToBinary().data1 < b.ToBinary().data1) return -1;
		return 0;
	}
};
template<>
struct SerDe<Guid::GuidData, true> {
	using Value = Guid::GuidData;
	static Value Get(vstd::span<vbyte const>& sp) {
		return Value{
			SerDe<uint64, true>::Get(sp),
			SerDe<uint64, true>::Get(sp)};
	}
	static void Set(Value const& data, vector<vbyte>& arr) {
		SerDe<uint64, true>::Set(data.data0, arr);
		SerDe<uint64, true>::Set(data.data1, arr);
	}
};

template<bool reverseBytes>
struct SerDe<Guid, reverseBytes> {
	using Value = Guid;
	static Value Get(vstd::span<vbyte const>& sp) {
		return SerDe<Guid::GuidData, reverseBytes>::Get(sp);
	}
	static void Set(Value const& data, vector<vbyte>& arr) {
		SerDe<Guid::GuidData, reverseBytes>::Set(data.ToBinary(), arr);
	}
};
}// namespace vstd