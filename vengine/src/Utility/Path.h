#pragma once
#include <Common/Common.h>
class VENGINE_DLL_COMMON Path {
private:
	vstd::string pathData;

public:
	static void SeparatePath(vstd::string_view path, vstd::vector<vstd::string>& blocks);
	Path(vstd::string path);
	Path(Path const& absolutePath, vstd::string_view relativePath);
	Path() {}
	bool IsEmpty() const {
		return pathData.empty();
	}
	static Path GetProgramPath();
	Path GetParentLevelPath() const;
	Path GetSubLevelPath(vstd::string_view subName) const;
	bool IsFile() const;
	bool IsDirectory() const;
	bool Exists() const;
	vstd::string const& GetPathStr() const {
		return pathData;
	}
	vstd::string GetExtension();
	Path& operator=(Path const& v);
	Path& operator=(vstd::string path);
	bool operator==(Path const& a) const;
	bool operator!=(Path const& a) const;
	bool IsSubPathOf(Path const& parentPath) const;
	bool IsParentPathOf(Path const& subPath) const;
	vstd::string TryGetSubPath(Path const& subPath) const;
	void TryCreateDirectory();
};
namespace vstd {
template<>
struct hash<Path> {
	size_t operator()(Path const& p) const noexcept {
		return hash<vstd::string>()(p.GetPathStr());
	}
};
template<>
struct compare<Path> {
	int32 operator()(Path const& a, Path const& b) const noexcept {
		return compare<vstd::string>()(a.GetPathStr(), b.GetPathStr());
	}
};
}// namespace vstd