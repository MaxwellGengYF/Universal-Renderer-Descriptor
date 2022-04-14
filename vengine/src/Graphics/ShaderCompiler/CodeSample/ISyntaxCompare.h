#pragma once
#include <Common/functional.h>
#include <Common/Common.h>
#include <Utility/StringUtility.h>
class ISyntaxCompare
{
public:
	enum class Step : uint8_t
	{
		Continue,
		NotAvaliable,
		Avaliable
	};
private:
	vstd::vector<ISyntaxCompare*> subSyntax;
	bool neccessary;
	bool captured;
	bool SelfSyntaxMatch(
		ISyntaxCompare* parent,
		char const*& currentPtr,
		char const* endPtr
	);
protected:

	virtual Step Match(
		ISyntaxCompare* parent,
		char const*& currentPtr,
		char const* endPtr
	) = 0;
	virtual void Clear() = 0;

public:
	bool IsCaptured() const {
		return captured;
	}
	bool IsNeccessary() const { return neccessary; }
	void AddSubSyntax(ISyntaxCompare* syn);
	void ClearSynbSyntex();
	ISyntaxCompare(bool neccessary);
	bool SyntaxMatch(
		vstd::string_view const& str
	)
	{
		if (str.size() == 0) return false;
		char const* start = str.data();
		char const* end = start + str.size();
		return SelfSyntaxMatch(nullptr, start, end);
	}
	void SyntaxClear();
	virtual ~ISyntaxCompare();
	DECLARE_VENGINE_OVERRIDE_OPERATOR_NEW
	KILL_COPY_CONSTRUCT(ISyntaxCompare)
};