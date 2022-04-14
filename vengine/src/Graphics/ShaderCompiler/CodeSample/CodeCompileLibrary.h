#pragma once
#include <Utility/StringUtility.h>
#include <bitset>
struct PreloadData
{
	using BitSet = std::bitset<std::numeric_limits<char>::max()>;
	BitSet IsWord;
	BitSet IsOperator;
	BitSet IsSpace;
	PreloadData();
	static PreloadData preloadData;

};
class SplitCode
{
	
	bool isWord = true;
public:
	CharCutState operator()(char c);
};

class SplitMacro
{
public:
	enum State
	{
		IsWord,
		IsOther,
		IsOperator
	};
	State state = State::IsWord;
	CharCutState operator()(char c);
};

class SplitSpace {
	bool isSpace = true;

public:
	CharCutState operator()(char c);
};

struct KeywordStruct
{
private:
	vstd::HashMap<vstd::string_view, uint> keyword_chunk;
	uint value = 0;

public:
	static KeywordStruct hlslTypeKeyword;
	static KeywordStruct hlslTypeSeparatedKeyword;
	KeywordStruct();
	void Add(std::initializer_list<char const*> lst);
	uint IsKeyWord(vstd::string const& word) const;
	uint IsKeyWord(vstd::string_view const& word) const;
};