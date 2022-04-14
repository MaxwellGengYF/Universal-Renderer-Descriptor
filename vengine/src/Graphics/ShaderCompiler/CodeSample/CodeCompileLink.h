#pragma once
#include <Common/Common.h>
struct CodeChunk
{
	enum CodeType
	{
		CodeType_Word,
		CodeType_Operators
	};
	CodeType type;
	vstd::string_view chunk;
	bool operator==(CodeChunk const& c) const
	{
		return type == c.type && chunk == c.chunk;
	}
	bool operator!=(CodeChunk const& c) const
	{
		return !operator==(c);
	}
};
namespace vengine
{
	template <>
	struct hash<CodeChunk>
	{
		size_t operator()(CodeChunk const& c) const
		{
			static const hash<uint> uintHash;
			static const hash<vstd::string_view> strHash;
			return (uintHash(c.type) << 16) ^ strHash(c.chunk);
		}
	};
}
template <typename T>
class CodeLogic
{
	funcPtr_t<bool(T*, vstd::string_view const&)> checkFunction;
	CodeLogic<T>* anyWordLogic = nullptr;
	vstd::HashMap<CodeChunk, CodeLogic<T>*> nextLevels;
	Pool<CodeLogic<T>, true, false>* pool;

public:
	CodeLogic(Pool<CodeLogic<T>, true, false>* pool) : pool(pool), nextLevels(32)
	{

	}
	CodeLogic<T>* AddRule(CodeChunk const& targetRule, funcPtr_t<bool(T*, vstd::string_view const&)> func)
	{
		CodeLogic<T>* newLogic = pool->New(pool);
		nextLevels.ForceEmplace(targetRule, newLogic);
		newLogic->checkFunction = func;
		return newLogic;
	}
	CodeLogic<T>* AddAnyWordLogic(funcPtr_t<bool(T*, vstd::string_view const&)> func)
	{
		if (anyWordLogic)
		{
			pool->Delete(anyWordLogic);
		}
		CodeLogic<T>* newLogic = pool->New(pool);
		anyWordLogic = newLogic;
		newLogic->pool = pool;
		newLogic->checkFunction = func;
		return newLogic;
	}
	struct Stack
	{
		CodeLogic<T>* logic;
		bool isOperator;
		uint operatorIndex;
		uint recordIndex = 0;
		CodeChunk chunk;
		CodeChunk* ckPtr;
		bool IsFinished()
		{
			return (logic->anyWordLogic == nullptr && logic->nextLevels.Size() == 0);
		}
		CodeLogic<T>* Forward(CodeChunk const& ck, T* ptr)
		{
			chunk = ck;
			isOperator = ck.type == CodeChunk::CodeType_Operators;
			if (isOperator)
			{
				operatorIndex = 1;
				for (; operatorIndex <= chunk.chunk.size(); operatorIndex++)
				{
					CodeChunk newChunk = { CodeChunk::CodeType_Operators, vstd::string_view(chunk.chunk.c_str(), operatorIndex) };
					auto ite = logic->nextLevels.Find(newChunk);
					if (ite && ite.Value()->checkFunction(ptr, newChunk.chunk))
					{
						operatorIndex++;
						return ite.Value();
					}
				}
				return nullptr;
			}
			else
			{
				if (logic->anyWordLogic && logic->anyWordLogic->checkFunction(ptr, ck.chunk))
				{
					return logic->anyWordLogic;
				}
				operatorIndex = chunk.chunk.size();
				auto ite = logic->nextLevels.Find(ck);
				if (ite && ite.Value()->checkFunction(ptr, ck.chunk))
				{
					return ite.Value();
				}
				return nullptr;
			}
		}
		CodeLogic<T>* Back(T* ptr)
		{
			if (isOperator)
			{
				for (; operatorIndex <= chunk.chunk.size(); operatorIndex++)
				{
					CodeChunk newChunk = { CodeChunk::CodeType_Operators, vstd::string_view(chunk.chunk.c_str(), operatorIndex) };
					auto ite = logic->nextLevels.Find(newChunk);
					if (ite && ite.Value()->checkFunction(ptr, newChunk.chunk))
					{
						operatorIndex++;
						return ite.Value();
					}
				}
				return nullptr;
			}
			else
			{
				if (logic->anyWordLogic && logic->anyWordLogic->checkFunction(ptr, chunk.chunk))
				{
					return logic->anyWordLogic;
				}
				return nullptr;
			}
		}
		Stack(CodeLogic<T>* l) : logic(l) {}
	};
	void ExecuteLogic(T* ptr, CodeChunk* chunk, CodeChunk* end)
	{
		vstd::vector<Stack*> nextLevels;
		Pool<Stack, true, false> pool(256);
		
		
		while (chunk < end)
		{
			bool isForward = true;
			uint offsetIndex = 0;
			CodeChunk currentChunk = *chunk;
			auto GenerateNextChunk = [&](uint newOffsetIndex)->void
			{
				if (newOffsetIndex > chunk->chunk.size())
				{
					chunk++;
					currentChunk = *chunk;
					offsetIndex = 0;
				}
				else
				{
					currentChunk.type = chunk->type;
					currentChunk.chunk = {
						chunk->chunk.c_str() + offsetIndex,
						chunk->chunk.size() - offsetIndex
					};
				}
			};
			for (auto& i : nextLevels)
			{
				pool.Delete(i);
			}
			nextLevels.clear();
			nextLevels.push_back(pool.New(this));
			while (!nextLevels.empty())
			{
				auto&& current = *(nextLevels.end() - 1);
				if (isForward)
				{
					if (current->IsFinished())
					{
						break;
					}
					current->recordIndex = offsetIndex;
					current->ckPtr = chunk;
					if (chunk >= end)
						break;
					auto nextPtr = current->Forward(currentChunk, ptr);
					if (nextPtr)
					{
						GenerateNextChunk(offsetIndex + current->operatorIndex);
						nextLevels.push_back(pool.New(nextPtr));
					}
					else
					{

						pool.Delete(nextLevels.erase_last());
						isForward = false;
					}
				}
				else
				{
					offsetIndex = current->recordIndex;
					chunk = current->ckPtr;
					if (chunk >= end)
						break;
					auto nextPtr = current->Back(ptr);
					if (nextPtr)
					{
						GenerateNextChunk(offsetIndex + current->operatorIndex);
						nextLevels.push_back(pool.New(nextPtr));
						isForward = true;
					}
					else
					{
						pool.Delete(nextLevels.erase_last());
					}
				}
			}
		}
	}
};