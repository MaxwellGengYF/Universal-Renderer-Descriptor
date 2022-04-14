#pragma once
#include <Common/Common.h>
#include <Common/functional.h>
namespace vstl {

class FileSystem {
public:
	struct FileTime {
		uint64 creationTime;
		uint64 lastAccessTime;
		uint64 lastWriteTime;
	};
	static bool IsFileExists(vstd::string const& path);
	static void GetFiles(
		vstd::string const& path,
		vstd::function<bool(vstd::string&&)> const& callBack,
		bool recursionFolder);
};
}// namespace vstl