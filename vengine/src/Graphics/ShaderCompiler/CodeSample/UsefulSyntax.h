#pragma once
#include <Graphics/ShaderCompiler/CodeSample/ISyntaxCompare.h>
class SyntaxBase final : public ISyntaxCompare
{
public:
	SyntaxBase() : ISyntaxCompare(true)
	{}
	virtual Step Match(
		ISyntaxCompare* parent,
		char const*&, char const*
	)
	{
		return Step::Avaliable;
	}
	virtual void Clear() {}
};
class WordSyntax final : public ISyntaxCompare
{
public:
	vstd::string word;
	WordSyntax(bool neccessary) : ISyntaxCompare(neccessary)
	{}
	virtual Step Match(
		ISyntaxCompare* parent,
		char const*& curPtr, char const* endPtr
	)
	{
		if (StringUtil::IsCharSpace(*curPtr))
		{
			++curPtr;
			return Step::Continue;
		}
		char const* start = curPtr;
		for (; curPtr != endPtr; ++curPtr)
		{
			if (!(StringUtil::IsCharCharacter(*curPtr) || StringUtil::IsCharNumber(*curPtr) || *curPtr == '_')
				|| StringUtil::IsCharSpace(*curPtr)){
				word.append(start, size_t(curPtr - start));
				return Step::Avaliable;
			}
		}
		word.append(start, size_t(curPtr - start));
		return Step::Avaliable;
	}
	virtual void Clear() {
		word.clear();
	}

};
class CoverAreaSyntax final : public ISyntaxCompare
{
public:
	vstd::string insideString;
	char start;
	char endChar;
	CoverAreaSyntax(
		char start,
		char end,
		bool neccessary) : ISyntaxCompare(neccessary), start(start), endChar(end)
	{}
	virtual Step Match(
		ISyntaxCompare* parent,
		char const*& curPtr,
		char const* end
	)
	{
		if (StringUtil::IsCharSpace(*curPtr))
		{
			++curPtr;
			return Step::Continue;
		}
		if (*curPtr != start)
		{
			return Step::NotAvaliable;
		}
		curPtr++;
		char const* begChar = curPtr;
		for (; curPtr < end; ++curPtr)
		{
			if (*curPtr == endChar)
			{
				insideString.append(begChar, size_t(curPtr - begChar));
				++curPtr;
				return Step::Avaliable;
			}
		}
		return Step::NotAvaliable;
	}
	virtual void Clear()
	{
		insideString.clear();
	}
};
class CutAreaSyntax final : public ISyntaxCompare
{
public:
	char c;
	CutAreaSyntax(char c, bool neccessary) : ISyntaxCompare(neccessary), c(c)
	{

	}
	virtual Step Match(
		ISyntaxCompare* parent,
		char const*& curPtr,
		char const* endPtr
	)
	{
		if (StringUtil::IsCharSpace(*curPtr))
		{
			++curPtr;
			return Step::Continue;
		}
		if (*curPtr == c)
		{
			++curPtr;
			return Step::Avaliable;
		}
		return Step::NotAvaliable;
	};
	virtual void Clear()
	{

	};
};