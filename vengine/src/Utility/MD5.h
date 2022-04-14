#pragma once
#include <Common/Common.h>
#include <Network/FunctionSerializer.h>
namespace vstd {
static constexpr size_t MD5_SIZE = 16;

VENGINE_DLL_COMMON std::array<vbyte, MD5_SIZE> GetMD5FromString(vstd::string const& str);
VENGINE_DLL_COMMON std::array<vbyte, MD5_SIZE> GetMD5FromArray(vstd::span<vbyte> data);
//Used for unity
class VENGINE_DLL_COMMON MD5 {
public:
	struct MD5Data {
		uint64 data0;
		uint64 data1;
	};

private:
	MD5Data data;

public:
	MD5Data const& ToBinary() const { return data; }
	MD5(vstd::string const& str);
	MD5(vstd::string_view str);
	MD5(vstd::span<vbyte const> bin);
	MD5(MD5 const&) = default;
	MD5(MD5&&) = default;
	MD5(MD5Data const& data);
	vstd::string ToString(bool upper = true) const;
	template<typename T>
	MD5& operator=(T&& t) {
		this->~MD5();
		new (this) MD5(std::forward<T>(t));
		return *this;
	}
	~MD5() = default;
	bool operator==(MD5 const& m) const;
	bool operator!=(MD5 const& m) const;
};
template<>
struct hash<MD5::MD5Data> {
	size_t operator()(MD5::MD5Data const& m) const {
		return Hash::CharArrayHash(&m.data0, sizeof(MD5::MD5Data));
	}
};
template<>
struct hash<MD5> {
	size_t operator()(MD5 const& m) const {
		static hash<MD5::MD5Data> dataHasher;
		return dataHasher(m.ToBinary());
	}
};
template<bool reverseBytes>
struct SerDe<MD5, reverseBytes> {
	static MD5 Get(vstd::span<vbyte const>& sp) {
		MD5::MD5Data data;
		data.data0 = SerDe<uint64, reverseBytes>::Get(sp);
		data.data1 = SerDe<uint64, reverseBytes>::Get(sp);
		return MD5(data);
	}
	static void Set(MD5 const& data, vector<vbyte>& arr) {
		auto&& dd = data.ToBinary();
		SerDe<uint64, reverseBytes>::Set(dd.data0, arr);
		SerDe<uint64, reverseBytes>::Set(dd.data1, arr);
	}
};
template<>
struct compare<MD5> {
	int32 operator()(MD5 const& a, MD5 const& b) const {
		if (a.ToBinary().data0 > b.ToBinary().data0) return 1;
		if (a.ToBinary().data0 < b.ToBinary().data0) return -1;
		if (a.ToBinary().data1 > b.ToBinary().data1) return 1;
		if (a.ToBinary().data1 < b.ToBinary().data1) return -1;
		return 0;
	}
};
}// namespace vstd