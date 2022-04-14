#pragma once
#include <Common/vector.h>
#include <Common/vstring.h>
#include <Common/HashMap.h>
#include <Common/Memory.h>
#include <Common/MetaLib.h>

struct ISeparateRule
{
	enum class State : uint8_t
	{
		None = 0,
		Begin = 1,
		End = 2
	};
	virtual State CheckState(vstd::string const& str, uint64 iteCount) = 0;
	virtual ~ISeparateRule() {}
	ISeparateRule() {}
	DECLARE_VENGINE_OVERRIDE_OPERATOR_NEW
	KILL_COPY_CONSTRUCT(ISeparateRule)
};
struct StringChunk final
{
	vstd::vector<StringChunk*> childChunk;
	vstd::string dataStr;
	vstd::Type ruleType;
	void Seperate(std::initializer_list<ISeparateRule*> const& rules);
	~StringChunk();
	DECLARE_VENGINE_OVERRIDE_OPERATOR_NEW
};