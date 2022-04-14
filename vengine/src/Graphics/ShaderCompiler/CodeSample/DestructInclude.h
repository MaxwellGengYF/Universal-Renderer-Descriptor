#pragma once
#include <Common/Common.h>
#include <Utility/Path.h>
struct DestructInclude
{
private:
	static bool ReadFile(Path const& path, vstd::HashMap<Path, bool>& exclusiveMap, bool pragmaOnce, vstd::string& codeDest);
public:
	static bool ReadFile(Path const& originPath, vstd::string& codeDest);
	KILL_COPY_CONSTRUCT(DestructInclude)
};