

#include <Graphics/ShaderCompiler/CodeSample/DestructInclude.h>
#include <Utility/StringUtility.h>
bool DestructInclude::ReadFile(Path const& path, vstd::HashMap<Path, bool>& exclusiveMap, bool pragmaOnce, vstd::string& codeDest)
{
	if (pragmaOnce)
	{
		if (exclusiveMap.Find(path))
		{
			return true;
		}
		else
			exclusiveMap.Emplace(path);
	}
	vstd::string fileData;

	if (!StringUtil::ReadStringFromFile(path.GetPathStr(), fileData))
		return false;
	vstd::vector<StringUtil::CodeChunk> allCodeChunks;
	if (fileData.empty()) return true;

	StringUtil::SampleCodeFile(fileData, allCodeChunks);
	vstd::string str;
	char const* const include = "#include";
	size_t const includeSize = strlen(include);
	vstd::vector<vstd::string_view> lines;
	for (uint64 i = 0; i < allCodeChunks.size() - 1; ++i)
	{
		auto& chunk = allCodeChunks[i];
		if (chunk.type == StringUtil::CodeType::Comment) continue;
		if (chunk.type == StringUtil::CodeType::Code)
		{
			StringUtil::CutToLine(chunk.str, lines);
			if (lines.empty())
			{
				codeDest += '\n';
				continue;
			}
			StringUtil::CullCharacater(*(lines.end() - 1), str, {
				' ', '\t', '\n', '\\', '\r'
				}
			);
			auto& nextChunk = allCodeChunks[i + 1];
			if (str == include && nextChunk.type == StringUtil::CodeType::String)
			{
				for (uint g = 0; g < lines.size() - 1; ++g)
				{
					codeDest += lines[g];
				}
				++i;
				Path includePath(path.GetParentLevelPath(), vstd::string(nextChunk.str.data() + 1, nextChunk.str.data() + nextChunk.str.size() - 1));
				codeDest += '\n';
				if (!ReadFile(includePath, exclusiveMap, pragmaOnce, codeDest)) 
					return false;
				continue;
			}
		}
		codeDest += chunk.str;
	}
	auto& end = *(allCodeChunks.end() - 1);
	if (end.type != StringUtil::CodeType::Comment)
	{
		codeDest += end.str;
	}
	return true;
}

bool DestructInclude::ReadFile(Path const& originPath, vstd::string& codeDest)
{
	vstd::string fileData;
	codeDest.clear();
	vstd::HashMap<Path, bool> exclusivePath(32);
	return ReadFile(originPath, exclusivePath, true, codeDest);
}