
#include <Utility/StringUtility.h>
#include <bitset>
#include <Utility/BinaryReader.h>
void StringUtil::IndicesOf(vstd::string_view str, vstd::string_view sign, vstd::vector<uint>& v) {
	v.clear();
	if (str.empty()) return;
	int32_t count = str.length() - sign.length() + 1;
	v.reserve(10);
	for (int32_t i = 0; i < count; ++i) {
		bool success = true;
		for (int32_t j = 0; j < sign.length(); ++j) {
			if (sign[j] != str[i + j]) {
				success = false;
				break;
			}
		}
		if (success)
			v.push_back(i);
	}
}

struct CharControl {
	std::bitset<std::numeric_limits<char>::max()> spaceChar;

	CharControl() {
		spaceChar[(uint8_t)' '] = 1;
		spaceChar[(uint8_t)'\t'] = 1;
		spaceChar[(uint8_t)'\r'] = 1;
		spaceChar[(uint8_t)'\n'] = 1;
		spaceChar[(uint8_t)'\\'] = 1;
	}
};
static CharControl charControl;
bool StringUtil::IsCharSpace(char c) {
	return charControl.spaceChar[(uint8_t)c];
}
bool StringUtil::IsCharNumber(char c) {
	return c >= '0' && c <= '9';
}
bool StringUtil::IsCharCharacter(char c) {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}
bool StringUtil::IsCharAvaliableCode(char c) {
	char cs[] = {' ', '\t', '\r', '\n', '\\'};
	for (auto& i : cs) {
		if (i == c) return false;
	}
	return true;
}
void StringUtil::IndicesOf(vstd::string_view str, char sign, vstd::vector<uint>& v) {
	v.clear();
	int32_t count = str.length();
	v.reserve(10);
	for (int32_t i = 0; i < count; ++i) {
		if (sign == str[i]) {
			v.push_back(i);
		}
	}
}
void StringUtil::CutToLine(vstd::string_view str, vstd::vector<vstd::string_view>& lines) {
	lines.clear();
	auto ite = Split(str, {"\n"_sv, "\r\n"_sv});
	for (auto&& i : ite) {
		lines.push_back(i);
	}
}
int64 StringUtil::GetFirstIndexOf(vstd::string_view str, char sign) {
	int64 count = str.length();
	for (int64 i = 0; i < count; ++i) {
		if (sign == str[i]) {
			return i;
		}
	}
	return -1;
}
void StringUtil::SplitCodeString(
	char const* beg,
	char const* end,
	vstd::vector<vstd::string_view>& results,
	void* ptr,
	vstd::funcPtr_t<CharCutState(void*, char)> func) {
	for (char const* i = beg; i < end; ++i) {
		CharCutState state = func(ptr, *i);
		switch (state) {
			case CharCutState::Different:
				if (beg < i)
					results.emplace_back(beg, i);
				beg = i;
				break;
			case CharCutState::Cull:
				if (beg < i)
					results.emplace_back(beg, i);
				beg = i + 1;
				break;
		}
	}
	if (beg < end)
		results.emplace_back(beg, end);
}
int64 StringUtil::GetFirstIndexOf(vstd::string_view str, vstd::string_view sign) {
	int64 count = str.length() - sign.length() + 1;
	for (int64 i = 0; i < count; ++i) {
		bool success = true;
		for (int64 j = 0; j < sign.length(); ++j) {
			if (sign[j] != str[i + j]) {
				success = false;
				break;
			}
		}
		if (success)
			return i;
	}
	return -1;
}
void StringUtil::GetDataFromAttribute(vstd::string_view str, vstd::string& result) {
	int32_t firstIndex = GetFirstIndexOf(str, '[');
	result.clear();
	if (firstIndex < 0) return;
	result.reserve(5);
	for (int32_t i = firstIndex + 1; str[i] != ']' && i < str.length(); ++i) {
		result.push_back(str[i]);
	}
}
void StringUtil::GetDataFromBrackets(vstd::string_view str, vstd::string& result) {
	int32_t firstIndex = GetFirstIndexOf(str, '<');
	result.clear();
	if (firstIndex < 0) return;
	result.reserve(5);
	for (int32_t i = firstIndex + 1; str[i] != '>' && i < str.length(); ++i) {
		result.push_back(str[i]);
	}
}
int64 StringUtil::StringToInteger(vstd::string_view str) {
	if (str.empty()) return 0;
	uint i;
	int64 value = 0;
	int32_t rate;
	if (str[0] == '-') {
		rate = -1;
		i = 1;
	} else {
		rate = 1;
		i = 0;
	}
	for (; i < str.length(); ++i) {
		value *= 10;
		value += (int32_t)str[i] - 48;
	}
	return value * rate;
}
double StringUtil::StringToFloat(vstd::string_view str) {
	if (str.empty()) return 0;
	uint i;
	double value = 0;
	int32_t rate;
	if (str[0] == '-') {
		rate = -1;
		i = 1;
	} else {
		rate = 1;
		i = 0;
	}
	for (; i < str.length(); ++i) {
		auto c = str[i];
		if (c == '.') {
			i++;
			break;
		}
		value *= 10;
		value += (int32_t)c - 48;
	}
	double afterPointRate = 1;
	for (; i < str.length(); ++i) {
		afterPointRate *= 0.1;
		value += afterPointRate * ((int32_t)str[i] - 48);
	}
	return value * rate;
}
inline void mtolower(char& c) {
	if ((c >= 'A') && (c <= 'Z'))
		c = c + ('a' - 'A');
}
inline void mtoupper(char& c) {
	if ((c >= 'a') && (c <= 'z'))
		c = c + ('A' - 'a');
}

inline char mtolower_value(char c) {
	if ((c >= 'A') && (c <= 'Z'))
		return c + ('a' - 'A');
	return c;
}
inline char mtoupper_value(char c) {
	if ((c >= 'a') && (c <= 'z'))
		return c + ('A' - 'a');
	return c;
}
void StringUtil::ToLower(vstd::string& str) {
	char* c = str.data();
	const uint size = str.length();
	for (uint i = 0; i < size; ++i) {
		mtolower(c[i]);
	}
}
void StringUtil::ToUpper(vstd::string& str) {
	char* c = str.data();
	const uint size = str.length();
	for (uint i = 0; i < size; ++i) {
		mtoupper(c[i]);
	}
}

vstd::string StringUtil::ToLower(vstd::string_view str) {
	vstd::string s;
	s.resize(str.size());
	for (auto i : vstd::range(str.size())) {
		auto&& v = s[i];
		v = str[i];
		mtolower(v);
	}
	return s;
}
vstd::string StringUtil::ToUpper(vstd::string_view str) {
	vstd::string s;
	s.resize(str.size());
	for (auto i : vstd::range(str.size())) {
		auto&& v = s[i];
		v = str[i];
		mtoupper(v);
	}
	return s;
}

bool StringUtil::EqualIgnoreCapital(vstd::string_view a, vstd::string_view b) {
	if (a.size() != b.size()) return false;
	for (auto i : vstd::range(a.size())) {
		if (mtoupper_value(a[i]) != mtoupper_value(b[i])) return false;
	}
	return true;
}

vstd::string_view StringUtil::GetExtension(vstd::string_view path) {
	char const* ptr = path.data() + path.length() - 1;
	for (; ptr != path.data(); --ptr) {
		if (*ptr == '.') {
			return vstd::string_view(ptr + 1, path.data() + path.length());
		}
	}
	return vstd::string_view(path);
}

void StringUtil::TransformWCharToChar(
	wchar_t const* src,
	char* dst,
	size_t sz) {
	for (size_t i = 0; i < sz; ++i) {
		dst[i] = static_cast<char>(src[i]);
	}
}

vstd::string_view CharSplitIterator::operator*() const {
	return result;
}
vstd::string_view CharsSplitIterator::operator*() const {
	return result;
}
void CharSplitIterator::operator++() {
	char const* start = curPtr;
	while (curPtr != endPtr) {
		if (*curPtr == sign) {
			if (start == curPtr) {
				++curPtr;
				start = curPtr;
				continue;
			}
			result = vstd::string_view(start, curPtr);
			++curPtr;
			return;
		}
		++curPtr;
	}
	if (endPtr == start) {
		result = vstd::string_view();
	} else {
		result = vstd::string_view(start, endPtr);
	}
}
void CharsSplitIterator::operator++() {
	char const* start = curPtr;
	auto check = [&](char const* c) -> size_t {
		for (auto&& i : signs) {
			if (endPtr - c < i.size()) continue;
			if (vstd::string_view(c, c + i.size()) == i) {
				return i.size();
			}
		}
		return 0;
	};
	while (curPtr != endPtr) {
		if (auto ct = check(curPtr)) {
			if (start == curPtr) {
				++curPtr;
				start = curPtr;
				continue;
			}
			result = vstd::string_view(start, curPtr);
			curPtr += ct;
			return;
		}
		++curPtr;
	}
	if (endPtr == start) {
		result = vstd::string_view();
	} else {
		result = vstd::string_view(start, endPtr);
	}
}
bool CharSplitIterator::operator==(vstd::IteEndTag) const {
	return result.size() == 0;
}
bool CharsSplitIterator::operator==(vstd::IteEndTag) const {
	return result.size() == 0;
}

vstd::string_view StrVSplitIterator::operator*() const {
	return result;
}
void StrVSplitIterator::operator++() {
	auto IsSame = [&](char const* ptr) {
		auto sz = endPtr - ptr;
		if (sz < sign.size()) return false;
		vstd::string_view value(ptr, sign.size());
		return value == sign;
	};
	char const* start = curPtr;
	while (curPtr < endPtr) {
		if (IsSame(curPtr)) {
			if (start == curPtr) {
				curPtr += sign.size();
				start = curPtr;
				continue;
			}
			result = vstd::string_view(start, curPtr);
			curPtr += sign.size();
			return;
		}
		++curPtr;
	}
	if (endPtr == start) {
		result = vstd::string_view();
	} else {
		result = vstd::string_view(start, endPtr);
	}
}
bool StrVSplitIterator::operator==(vstd::IteEndTag) const {
	return result.size() == 0;
}
namespace vstd::strutil_detail {
static char constexpr tab[] = {
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,//   0-15
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,//  16-31
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,//  32-47
	52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,//  48-63
	-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,		   //  64-79
	15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,//  80-95
	-1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,//  96-111
	41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1,// 112-127
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,// 128-143
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,// 144-159
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,// 160-175
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,// 176-191
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,// 192-207
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,// 208-223
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,// 224-239
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 // 240-255
};
static char constexpr tab1[] = {
	"ABCDEFGHIJKLMNOP"
	"QRSTUVWXYZabcdef"
	"ghijklmnopqrstuv"
	"wxyz0123456789+/"};
char const* get_inverse() {
	return &tab[0];
}
char const* get_alphabet() {

	return &tab1[0];
}

size_t encode(void* dest, void const* src, size_t len) {
	char* out = static_cast<char*>(dest);
	char const* in = static_cast<char const*>(src);
	auto const tab = get_alphabet();

	for (auto n = len / 3; n--;) {
		*out++ = tab[(in[0] & 0xfc) >> 2];
		*out++ = tab[((in[0] & 0x03) << 4) + ((in[1] & 0xf0) >> 4)];
		*out++ = tab[((in[2] & 0xc0) >> 6) + ((in[1] & 0x0f) << 2)];
		*out++ = tab[in[2] & 0x3f];
		in += 3;
	}

	switch (len % 3) {
		case 2:
			*out++ = tab[(in[0] & 0xfc) >> 2];
			*out++ = tab[((in[0] & 0x03) << 4) + ((in[1] & 0xf0) >> 4)];
			*out++ = tab[(in[1] & 0x0f) << 2];
			*out++ = '=';
			break;

		case 1:
			*out++ = tab[(in[0] & 0xfc) >> 2];
			*out++ = tab[((in[0] & 0x03) << 4)];
			*out++ = '=';
			*out++ = '=';
			break;

		case 0:
			break;
	}

	return out - static_cast<char*>(dest);
}

std::pair<size_t, size_t> decode(void* dest, char const* src, size_t len) {
	char* out = static_cast<char*>(dest);
	auto in = reinterpret_cast<unsigned char const*>(src);
	unsigned char c3[3], c4[4];
	int i = 0;
	int j = 0;

	auto const inverse = get_inverse();

	while (len-- && *in != '=') {
		auto const v = inverse[*in];
		if (v == -1)
			break;
		++in;
		c4[i] = v;
		if (++i == 4) {
			c3[0] = (c4[0] << 2) + ((c4[1] & 0x30) >> 4);
			c3[1] = ((c4[1] & 0xf) << 4) + ((c4[2] & 0x3c) >> 2);
			c3[2] = ((c4[2] & 0x3) << 6) + c4[3];

			for (i = 0; i < 3; i++)
				*out++ = c3[i];
			i = 0;
		}
	}

	if (i) {
		c3[0] = (c4[0] << 2) + ((c4[1] & 0x30) >> 4);
		c3[1] = ((c4[1] & 0xf) << 4) + ((c4[2] & 0x3c) >> 2);
		c3[2] = ((c4[2] & 0x3) << 6) + c4[3];

		for (j = 0; j < i - 1; j++)
			*out++ = c3[j];
	}

	return {out - static_cast<char*>(dest),
			in - reinterpret_cast<unsigned char const*>(src)};
}
size_t constexpr encoded_size(size_t n) {
	return 4 * ((n + 2) / 3);
}

/// Returns max bytes needed to decode a base64 string
size_t constexpr decoded_size(size_t n) {
	return n / 4 * 3;// requires n&3==0, smaller
}

}// namespace vstd::strutil_detail
void StringUtil::EncodeToBase64(vstd::span<vbyte const> binary, vstd::string& str) {
	using namespace vstd::strutil_detail;
	size_t oriSize = str.size();
	str.resize(oriSize + encoded_size(binary.size()));
	encode(str.data() + oriSize, binary.data(), binary.size());
}
void StringUtil::EncodeToBase64(vstd::span<vbyte const> binary, char* result) {
	using namespace vstd::strutil_detail;
	encode(result, binary.data(), binary.size());
}

void StringUtil::DecodeFromBase64(vstd::string_view str, vstd::vector<vbyte>& bin) {
	using namespace vstd::strutil_detail;
	size_t oriSize = bin.size();
	bin.reserve(oriSize + decoded_size(str.size()));
	auto destAndSrcSize = decode(bin.data() + oriSize, str.data(), str.size());
	bin.resize(oriSize + destAndSrcSize.first);
}

void StringUtil::DecodeFromBase64(vstd::string_view str, vbyte* size) {
	using namespace vstd::strutil_detail;
	decode(size, str.data(), str.size());
}

vstd::variant<int64, double> StringUtil::StringToNumber(vstd::string_view numStr) {
	auto pin = numStr.begin();
	auto error = []() {
		return false;
	};
	int64 integerPart = 0;
	vstd::optional<double> floatPart;
	vstd::optional<int64> ratePart;
	float sign = 1;
	auto GetInt =
		[&](int64& v,
			auto&& processFloat,
			auto&& processRate) -> bool {
		bool isNegative;
		if (numStr[0] == '-') {
			isNegative = true;
			pin++;
		} else if (numStr[0] == '+') {
			isNegative = false;
			pin++;
		} else {
			isNegative = false;
		}
		while (pin != numStr.end()) {
			if (*pin >= '0' && *pin <= '9') {
				v *= 10;
				v += (*pin) - 48;
				pin++;
			} else if (*pin == '.') {
				pin++;
				if (!processFloat()) return false;
				break;
			} else if (*pin == 'e') {
				pin++;
				if (!processRate()) return false;
				break;
			} else
				return error();
		}
		if (isNegative) {
			v *= -1;
			sign = -1;
		}
		return true;
	};
	auto GetFloat = [&](double& v, auto&& processRate) -> bool {
		double rate = 1;
		while (pin != numStr.end()) {
			if (*pin >= '0' && *pin <= '9') {
				rate *= 0.1;
				v += ((*pin) - 48) * rate;
				pin++;
			} else if (*pin == 'e') {
				pin++;
				return processRate();
			} else {
				return error();
			}
		}
		return true;
	};

	auto GetRate = [&]() -> bool {
		ratePart.New();
		return GetInt(*ratePart, error, error);
	};
	auto GetNumFloatPart = [&]() -> bool {
		floatPart.New();
		return GetFloat(*floatPart, GetRate);
	};
	if (!GetInt.operator()(
			integerPart,
			GetNumFloatPart,
			GetRate)) return {};
	if (ratePart || floatPart) {
		double value = (integerPart + (floatPart ? *floatPart : 0) * sign) * (ratePart ? pow(10, *ratePart) : 1);
		return value;
	} else {
		return integerPart;
	}
}
void StringUtil::CullCharacater(vstd::string_view const& source, vstd::string& dest, std::initializer_list<char> const& lists) {
	std::bitset<256> bit;
	for (auto i : lists) {
		bit[(vbyte)i] = 1;
	}
	dest.clear();
	char const* last = source.data();
	char const* end = last + source.size();
	size_t sz = 0;
	for (char const* ite = last; ite != end; ++ite) {
		if (bit[(vbyte)*ite]) {
			if (sz > 0) {
				dest.append(last, sz);
				sz = 0;
			}
			last = ite + 1;
		} else {
			sz++;
		}
	}
	if (sz > 0) {
		dest.append(last, sz);
	}
}
bool StringUtil::ReadStringFromFile(vstd::string const& path, vstd::string& str) {
	BinaryReader br(path);
	if (!br) {
		str.clear();
		return false;
	} else {
		str.clear();
		str.resize(br.GetLength());
		br.Read(str.data(), str.size());
		return true;
	}
}
namespace StringUtilGlobal {
template<bool containType>
void InputCommentFunc(
	vstd::vector<StringUtil::CodeChunk>& vec,
	StringUtil::CodeType type,
	char const*& lastPtr,
	char const*& ite,
	char const* end,
	char const* leftSign,
	size_t const leftSignSize,
	char const* rightSign,
	size_t const rightSignSize) {
	if (end - ite < leftSignSize) return;
	if (StringUtil::CompareCharArray(ite, leftSign, leftSignSize)) {
		if (
			reinterpret_cast<int64_t>(ite) - reinterpret_cast<int64_t>(lastPtr) > 0) {
			vec.emplace_back(lastPtr, ite, StringUtil::CodeType::Code);
		}
		char const* commentStart = ite;
		ite += leftSignSize;
		char const* iteEnd = end - (rightSignSize - 1);
		while (ite < iteEnd) {
			if (StringUtil::CompareCharArray(ite, rightSign, rightSignSize)) {
				ite += rightSignSize;
				if constexpr (containType)
					auto&& code = vec.emplace_back(commentStart, ite, type);
				lastPtr = ite;
				return;
			}
			ite++;
		}
		if (
			reinterpret_cast<int64_t>(end) - reinterpret_cast<int64_t>(commentStart) > 0) {
			vec.emplace_back(commentStart, end, type);
		}
		lastPtr = end;
	}
}
}// namespace StringUtilGlobal

void StringUtil::SampleCodeFile(vstd::string_view const& fileData, vstd::vector<CodeChunk>& results, bool separateCodeAndString, bool disposeComment) {
	using namespace StringUtilGlobal;
	results.clear();
	if (fileData.size() == 0) return;
	char const* begin = fileData.data();
	char const* end = fileData.data() + fileData.size();
	char const* codeStart = begin;
	auto CollectTailFunc = [&](vstd::vector<CodeChunk>& res) -> void {
		if (
			reinterpret_cast<int64_t>(end) - reinterpret_cast<int64_t>(codeStart) > 0) {
			res.emplace_back(codeStart, end, CodeType::Code);
		}
	};
	if (separateCodeAndString) {
		vstd::vector<CodeChunk> codeAndComments;
		for (char const* ite = begin; ite < end; ++ite) {
			if (disposeComment) {
				InputCommentFunc<false>(
					codeAndComments,
					CodeType::Comment,
					codeStart,
					ite,
					end,
					"//",
					2,
					"\n",
					1);
				InputCommentFunc<false>(
					codeAndComments,
					CodeType::Comment,
					codeStart,
					ite,
					end,
					"/*",
					2,
					"*/",
					2);
			} else {
				InputCommentFunc<true>(
					codeAndComments,
					CodeType::Comment,
					codeStart,
					ite,
					end,
					"//",
					2,
					"\n",
					1);
				InputCommentFunc<true>(
					codeAndComments,
					CodeType::Comment,
					codeStart,
					ite,
					end,
					"/*",
					2,
					"*/",
					2);
			}
		}
		CollectTailFunc(codeAndComments);
		for (auto& chunk : codeAndComments) {
			if (chunk.type == CodeType::Code) {
				begin = chunk.str.data();
				codeStart = begin;
				end = chunk.str.data() + chunk.str.size();
				for (char const* ite = begin; ite < end; ++ite) {
					InputCommentFunc<true>(
						results,
						CodeType::String,
						codeStart,
						ite,
						end,
						"\"",
						1,
						"\"",
						1);
					InputCommentFunc<true>(
						results,
						CodeType::String,
						codeStart,
						ite,
						end,
						"'",
						1,
						"'",
						1);
				}
				CollectTailFunc(results);
			} else {
				results.push_back(chunk);
			}
		}
	} else {
		for (char const* ite = begin; ite < end; ++ite) {
			if (disposeComment) {
				InputCommentFunc<false>(
					results,
					CodeType::Comment,
					codeStart,
					ite,
					end,
					"//",
					2,
					"\n",
					1);
				InputCommentFunc<false>(
					results,
					CodeType::Comment,
					codeStart,
					ite,
					end,
					"/*",
					2,
					"*/",
					2);
			} else {
				InputCommentFunc<true>(
					results,
					CodeType::Comment,
					codeStart,
					ite,
					end,
					"//",
					2,
					"\n",
					1);
				InputCommentFunc<true>(
					results,
					CodeType::Comment,
					codeStart,
					ite,
					end,
					"/*",
					2,
					"*/",
					2);
			}
		}
		CollectTailFunc(results);
	}
}