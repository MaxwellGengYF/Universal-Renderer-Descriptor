#pragma once
#include <Graphics/ShaderCompiler/CodeSample/StringChunk.h>
#include <Utility/StringUtility.h>
struct CommentSeparateRule : public ISeparateRule
{
	int leftedSize = 0;
	enum class CurrentState : uint8_t
	{
		None, DoubleSlash, SlashStar
	};
	CurrentState selfState = CurrentState::None;
	virtual State CheckState(vstd::string const& str, uint64 iteCount)
	{
		if ((leftedSize--) > 0)
			return State::None;
		if (iteCount + 1 < str.size())
		{

			switch (selfState)
			{
			case CurrentState::None:
				if (StringUtil::CompareCharArray(&str[iteCount], "//", 2))
				{
					leftedSize = 1;
					selfState = CurrentState::DoubleSlash;
					return State::Begin;
				}
				if (StringUtil::CompareCharArray(&str[iteCount], "/*", 2))
				{
					leftedSize = 1;
					selfState = CurrentState::SlashStar;
					return State::Begin;
				}
				break;
			case CurrentState::DoubleSlash:
				if (str[iteCount] == '\n')
				{
					selfState = CurrentState::None;
					return State::End;
				}
				break;

			case CurrentState::SlashStar:
				if (str[iteCount] == '*/')
				{
					selfState = CurrentState::None;
					return State::End;
				}
				break;
			}
		}
		return State::None;
	}
	CommentSeparateRule() {}
	KILL_COPY_CONSTRUCT(CommentSeparateRule)
};