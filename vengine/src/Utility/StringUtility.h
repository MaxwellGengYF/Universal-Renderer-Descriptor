#pragma once
#include <Common/Common.h>
#include <fstream>
struct VENGINE_DLL_COMMON CharSplitIterator {
	char const* curPtr;
	char const* endPtr;
	char sign;
	vstd::string_view result;
	vstd::string_view operator*() const;
	void operator++();
	bool operator==(vstd::IteEndTag) const;
};
struct VENGINE_DLL_COMMON CharsSplitIterator {
	char const* curPtr;
	char const* endPtr;
	vstd::span<vstd::string_view const> signs;
	vstd::string_view result;
	vstd::string_view operator*() const;
	void operator++();
	bool operator==(vstd::IteEndTag) const;
};
struct VENGINE_DLL_COMMON StrVSplitIterator {
	char const* curPtr;
	char const* endPtr;
	vstd::string_view sign;
	vstd::string_view result;
	vstd::string_view operator*() const;
	void operator++();
	bool operator==(vstd::IteEndTag) const;
};
template<typename SignT, typename IteratorType>
struct StrvIEnumerator {
	char const* curPtr;
	char const* endPtr;
	SignT sign;
	IteratorType begin() const {
		IteratorType c{curPtr, endPtr, sign};
		++c;
		return c;
	}
	vstd::IteEndTag end() const {
		return {};
	}
};
enum class CharCutState : uint8_t {
	Same,
	Different,
	Cull
};
class VENGINE_DLL_COMMON StringUtil {
private:
	StringUtil() = delete;
	KILL_COPY_CONSTRUCT(StringUtil)
public:
	enum class CodeType : uint8_t {
		Code,
		Comment,
		String
	};
	struct CodeChunk {
		vstd::string str;
		CodeType type;
		CodeChunk() {}
		CodeChunk(
			vstd::string str,
			CodeType type) : str(str), type(type) {}
		CodeChunk(
			char const* start,
			char const* end,
			CodeType type) : str(start, end), type(type) {}
	};
	static bool CompareCharArray(char const* p, char const* p1, size_t len) {
		return memcmp(p, p1, len) == 0;
	}

	static void IndicesOf(vstd::string_view str, vstd::string_view sign, vstd::vector<uint>& v);
	static void IndicesOf(vstd::string_view str, char, vstd::vector<uint>& v);
	static void CutToLine(vstd::string_view str, vstd::vector<vstd::string_view>& lines);
	static int64 GetFirstIndexOf(vstd::string_view str, vstd::string_view sign);
	static int64 GetFirstIndexOf(vstd::string_view str, char sign);
	static bool ReadStringFromFile(vstd::string const& path, vstd::string& str);
	static StrvIEnumerator<char, CharSplitIterator> Split(vstd::string_view str, char sign) {
		return StrvIEnumerator<char, CharSplitIterator>{str.data(), str.data() + str.size(), sign};
	}
	static bool IsCharSpace(char p);
	static bool IsCharNumber(char c);
	static bool IsCharCharacter(char c);
	static bool IsCharAvaliableCode(char c);
	static StrvIEnumerator<vstd::string_view, StrVSplitIterator> Split(vstd::string_view str, vstd::string_view sign) {
		return StrvIEnumerator<vstd::string_view, StrVSplitIterator>{str.data(), str.data() + str.size(), sign};
	}
	static StrvIEnumerator<vstd::span<vstd::string_view const>, CharsSplitIterator> Split(vstd::string_view str, vstd::span<vstd::string_view const> sign) {
		return StrvIEnumerator<vstd::span<vstd::string_view const>, CharsSplitIterator>{str.data(), str.data() + str.size(), sign};
	}
	static StrvIEnumerator<vstd::span<vstd::string_view const>, CharsSplitIterator> Split(vstd::string_view str, std::initializer_list<vstd::string_view> sign) {
		return StrvIEnumerator<vstd::span<vstd::string_view const>, CharsSplitIterator>{str.data(), str.data() + str.size(), vstd::span<vstd::string_view const>(sign.begin(), sign.size())};
	}
	static void SplitCodeString(
		char const* beg,
		char const* end,
		vstd::vector<vstd::string_view>& results,
		void* ptr,
		vstd::funcPtr_t<CharCutState(void*, char)> func);
	template<typename Func>
	static void SplitCodeString(
		char const* beg,
		char const* end,
		vstd::vector<vstd::string_view>& results,
		Func& func) {
		SplitCodeString(
			beg,
			end,
			results,
			&func,
			[](void* pp, char c) -> CharCutState {
				return static_cast<CharCutState>(((Func*)pp)->operator()(c));
			});
	}
	template<typename Func>
	static void SplitCodeString(
		char const* beg,
		char const* end,
		vstd::vector<vstd::string_view>& results,
		Func&& func) {
		SplitCodeString(
			beg,
			end,
			results,
			func);
	}
	template<typename Func>
	static void SplitCodeString(
		vstd::string const& str,
		vstd::vector<vstd::string_view>& results,
		Func& func) {
		results.clear();
		char const* beg = str.data();
		char const* end = beg + str.size();
		SplitCodeString<Func>(
			beg,
			end,
			results,
			func);
	}
	template<typename Func>
	static void SplitCodeString(
		vstd::string const& str,
		vstd::vector<vstd::string_view>& results,
		Func&& func) {
		SplitCodeString(
			str,
			results,
			func);
	}
	static void GetDataFromAttribute(vstd::string_view str, vstd::string& result);
	static void GetDataFromBrackets(vstd::string_view str, vstd::string& result);
	static int64 StringToInteger(vstd::string_view str);
	static double StringToFloat(vstd::string_view str);
	static vstd::variant<int64, double> StringToNumber(vstd::string_view numStr);
	static void ToLower(vstd::string& str);
	static void ToUpper(vstd::string& str);
	static void CullCharacater(vstd::string_view const& source, vstd::string& dest, std::initializer_list<char> const& lists);
	static void SampleCodeFile(vstd::string_view const& fileData, vstd::vector<CodeChunk>& results, bool separateCodeAndString = true, bool disposeComment = false);
	static void CullCodeSpace(vstd::string_view const& source, vstd::string& dest) {
		CullCharacater(
			source, dest,
			{' ', '\t', '\r', '\n', '\\'});
	}
	static vstd::string ToLower(vstd::string_view str);
	static vstd::string ToUpper(vstd::string_view str);
	static bool EqualIgnoreCapital(vstd::string_view a, vstd::string_view b);
	static vstd::string_view GetExtension(vstd::string_view path);
	static void EncodeToBase64(vstd::span<vbyte const> binary, vstd::string& result);
	static void EncodeToBase64(vstd::span<vbyte const> binary, char* result);
	static void DecodeFromBase64(vstd::string_view str, vstd::vector<vbyte>& result);
	static void DecodeFromBase64(vstd::string_view str, vbyte* size);
	static void TransformWCharToChar(
		wchar_t const* src,
		char* dst,
		size_t sz);
};