
#include <Common/Common.h>
#include <Graphics/ShaderCompiler/CodeSample/GetStructData.h>
#include <Utility/StringUtility.h>
#include <Graphics/ShaderCompiler/CodeSample/CodeCompileLibrary.h>
size_t GetStructData::GetSize(vstd::string const& str) const {
	auto ite = structSizes.Find(str);
	if (!ite) return 0;
	return ite.Value();
}
GetStructData::GetStructData(vstd::string const& str) : structSizes(64), str(str) {
	auto AddKey = [&](std::initializer_list<std::pair<char const*, size_t>> const& cst) {
		for (auto& i : cst) {
			structSizes.ForceEmplace(i.first, i.second);
		}
	};
	AddKey(
		{
			{"float", 4},
			{"float1", 4},
			{"float2", 8},
			{"float3", 12},
			{"float4", 16},
			{"float1x1", 4},
			{"float1x2", 8},
			{"float1x3", 12},
			{"float1x4", 16},
			{"float2x1", 8},
			{"float2x2", 16},
			{"float2x3", 24},
			{"float2x4", 32},
			{"float3x1", 12},
			{"float3x2", 24},
			{"float3x3", 36},
			{"float3x4", 48},
			{"float4x1", 16},
			{"float4x2", 32},
			{"float4x3", 48},
			{"float4x4", 64},
			{"uint", 4},
			{"uint1", 4},
			{"uint2", 8},
			{"uint3", 12},
			{"uint4", 16},
			{"uint1x1", 4},
			{"uint1x2", 8},
			{"uint1x3", 12},
			{"uint1x4", 16},
			{"uint2x1", 8},
			{"uint2x2", 16},
			{"uint2x3", 24},
			{"uint2x4", 32},
			{"uint3x1", 12},
			{"uint3x2", 24},
			{"uint3x3", 36},
			{"uint3x4", 48},
			{"uint4x1", 16},
			{"uint4x2", 32},
			{"uint4x3", 48},
			{"uint4x4", 64},
			{"int", 4},
			{"int1", 4},
			{"int2", 8},
			{"int3", 12},
			{"int4", 16},
			{"int1x1", 4},
			{"int1x2", 8},
			{"int1x3", 12},
			{"int1x4", 16},
			{"int2x1", 8},
			{"int2x2", 16},
			{"int2x3", 24},
			{"int2x4", 32},
			{"int3x1", 12},
			{"int3x2", 24},
			{"int3x3", 36},
			{"int3x4", 48},
			{"int4x1", 16},
			{"int4x2", 32},
			{"int4x3", 48},
			{"int4x4", 64},
		});
}
bool GetStructData::Parse() {
	maxSize = 0;
	vstd::vector<vstd::string_view> chunks;
	StringUtil::SplitCodeString(
		str, chunks,
		SplitCode());
	uint64 i = 0;
	vstd::string_view currentStruct;
	size_t* sizePtr = nullptr;
	auto GetName = [&]() -> bool {
		i++;
		if (i >= chunks.size())
			return false;
		currentStruct = chunks[i];
		sizePtr = &structSizes.ForceEmplace(currentStruct, 0).Value();
		return true;
	};
	auto GetChar = [&](auto&& chr) -> bool {
		i++;
		for (; i < chunks.size(); ++i) {
			if (chunks[i] == chr) {
				return true;
			}
		}
		return false;
	};
	auto GetAnyChar = [&](std::initializer_list<char> const& chars) -> size_t {
		auto sz = chars.size();
		i++;
		for (; i < chunks.size(); ++i) {
			auto v = chunks[i];
			for (size_t j = 0; j < sz; ++j) {
				if (v == chars.begin()[j]) return j;
			}
		}
		return -1;
	};
	auto SampleVariable = [&](bool& theEnd) -> bool {
		if (i >= chunks.size())
			return false;
		uint64 startIndex = i;
		size_t charCount = GetAnyChar({';', '}'});
		if (charCount < 0)
			return false;
		if (charCount == 1) {
			theEnd = true;
			return true;
		}
		if (chunks[startIndex] == ';' || chunks[startIndex] == '{')
			startIndex++;
		if (i >= chunks.size())
			return false;
		uint64 endIndex = i;
		vstd::HashMap<vstd::string_view, size_t>::Index ite;
		size_t varCount = 0;
		auto a = chunks[startIndex];
		auto b = chunks[endIndex];
		if (endIndex - startIndex == 2) {
			auto&& chunk = chunks[startIndex];
			ite = structSizes.Find(chunk);
			varCount = 1;
		} else if (endIndex - startIndex == 5)//int a[5];
		{
			ite = structSizes.Find(chunks[startIndex]);
			if (!ite)
				return false;
			vstd::string_view numberChunk = chunks[startIndex + 3];
			//TODO
			//Support Macro
			return StringUtil::StringToNumber(numberChunk).visit_or(false, [&](auto&& v) {
				varCount = v;
				return true;
			});
		} else {

			return false;
		}
		if (ite.Key() == currentStruct) {
			return false;//sturct cannot have self!
		}
		*sizePtr += ite.Value();
		return true;
	};
	for (; i < chunks.size();) {
		auto chunk = chunks[i];
		if (chunk == "struct") {
			if (!GetName())
				return false;
			auto a = chunks[i];
			if (!GetChar('{'))
				return false;
			bool value = false;
			a = chunks[i];
			while (!value) {
				if (!SampleVariable(value))
					return false;
			}
			if (*sizePtr > maxSize) maxSize = *sizePtr;
		} else {
			++i;
		}
	}
	return true;
}
GetStructData::~GetStructData() {
}