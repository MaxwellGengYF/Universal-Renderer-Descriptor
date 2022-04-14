#pragma once
#include <Common/Common.h>
#include <Common/functional.h>
#include <Database/Parser/StateRecorder.h>
#include <Utility/VGuid.h>
namespace toolhub::db::parser {
struct ParserStates {
	using BoolArrayType = typename StateRecorder<bool>::ArrayType;
	StateRecorder<bool> guidCheck;
	StateRecorder<bool> numCheck;
	StateRecorder<bool> pureNumCheck;
	StateRecorder<bool> spaces;
	StateRecorder<bool> keywordStates;

	ParserStates()
		: guidCheck(
			[&](BoolArrayType const& ptr) {
				vstd::string_view sv =
					"ABCDEFGHIJKLMNOP"
					"QRSTUVWXYZabcdef"
					"ghijklmnopqrstuv"
					"wxyz0123456789+/="_sv;
				for (auto i : sv) {
					ptr[i] = true;
				}
			}),
		  pureNumCheck(
			  [&](BoolArrayType const& ptr) {
				  vstd::string_view sv = "0123456789-+"_sv;
				  for (auto i : sv) {
					  ptr[i] = true;
				  }
			  }),
		  numCheck(
			  [&](BoolArrayType const& ptr) {
				  vstd::string_view sv = "0123456789.eE-+"_sv;
				  for (auto i : sv) {
					  ptr[i] = true;
				  }
			  }),
		  spaces(
			  [&](BoolArrayType const& ptr) {
				  ptr[' '] = true;
				  ptr['\t'] = true;
				  ptr['\r'] = true;
				  ptr['\n'] = true;
			  }),
		  keywordStates(
			  [&](BoolArrayType const& ptr) {
				  for (auto i : vstd::range('a', 'z' + 1)) {
					  ptr[i] = true;
				  }
				  for (auto i : vstd::range('A', 'Z' + 1)) {
					  ptr[i] = true;
				  }
			  })

	{
	}
};
}// namespace toolhub::db::parser
namespace vstd {
class VENGINE_DLL_COMMON FunctionExecutor final : public IOperatorNewBase {
	toolhub::db::parser::ParserStates recorders;

public:
	struct Element : public IOperatorNewBase {
		using VariantType = variant<
			int64,
			double,
			vstd::string,
			Guid,
			bool,
			std::nullptr_t,
			vector<unique_ptr<Element>>>;

		VariantType data;
		Element(VariantType&& varT)
			: data(std::move(varT)) {}
		Element(Element const&) = delete;
		Element(Element&&) = default;
	};
	enum class KeyWord : vbyte {
		True,
		False,
		Null
	};
	using FuncType = function<vstd::string(Element&&)>;

private:
	HashMap<vstd::string_view, KeyWord> keywords;
	HashMap<vstd::string, FuncType> funcMap;
	char const* GetNextChar(char const* ptr, char const* end) const;
	template<char beginEnd>
	bool GetString(char const*& ptr, char const* const end, vstd::string& str) const;
	bool GetElementVector(
		char const*& ptr,
		char const* const end,
		vector<unique_ptr<Element>>& elements) const;
	Element::VariantType ParseElement(char const*& ptr, char const* const end) const;

public:
	void Dispose();
	void Reset();
	void AddFunctor(vstd::string name, FuncType func);

	FunctionExecutor();
	vstd::string Execute(vstd::string_view cmd);
};
}// namespace vstd