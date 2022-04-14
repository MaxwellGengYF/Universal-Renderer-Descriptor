

#include <Common/Common.h>
#include <Graphics/ShaderCompiler/CodeSample/StringChunk.h>
#include <assert.h>
StringChunk::~StringChunk()
{
	for (auto& i : childChunk)
	{
		delete i;
	}
}
void StringChunk::Seperate(std::initializer_list<ISeparateRule*> const& rules)
{
	for (auto& i : childChunk)
	{
		delete i;
	}
	childChunk.clear();
	ISeparateRule* currentRule = nullptr;
	StringChunk* chunk = nullptr;
	for (uint64 i = 0; i < dataStr.size(); ++i)
	{

		ISeparateRule::State state;
		if (!currentRule)
		{
			for (auto& rule : rules)
			{
				state = rule->CheckState(dataStr, i);
				if (state != ISeparateRule::State::None)
				{
					currentRule = rule;
					break;
				}
			}
		}
		else
		{
			state = currentRule->CheckState(dataStr, i);
		}
		vstd::Type ruleType = currentRule ? vstd::Type(typeid(*currentRule)) : vstd::Type(nullptr);
		if (!chunk)
		{
			chunk = new StringChunk();
			chunk->ruleType = ruleType;
		}
		else if (chunk->ruleType != ruleType)
		{
			childChunk.push_back(chunk);
			chunk = new StringChunk();
			chunk->ruleType = ruleType;
		}
		chunk->dataStr.push_back(dataStr[i]);
		if (state == ISeparateRule::State::End)
		{
			childChunk.push_back(chunk);
			chunk = nullptr;
			currentRule = nullptr;
		}
	}
	if (chunk)
	{
		childChunk.push_back(chunk);
	}
}