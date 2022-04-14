

#include <Utility/Path.h>
#include <Utility/FileUtility.h>
#include <io.h>
Path::Path(vstd::string path) {
	operator=(std::move(path));
}
Path::Path(Path const& absolutePath, vstd::string_view relativePath) {
	Path curPath = absolutePath;
	vstd::vector<vstd::string> blocks;
	SeparatePath(relativePath, blocks);
	for (auto ite = blocks.begin(); ite != blocks.end(); ++ite) {
		if (*ite == "..") {
			curPath = curPath.GetParentLevelPath();
		} else if (*ite == ".") {
			continue;
		} else {
			curPath = curPath.GetSubLevelPath(*ite);
		}
	}
	pathData = curPath.pathData;
}

bool Path::operator==(Path const& a) const {
	return pathData == a.pathData;
}
bool Path::operator!=(Path const& a) const {
	return !operator==(a);
}

bool Path::Exists() const {
	return _access(pathData.c_str(), 0) != -1;
}

Path& Path::operator=(vstd::string path) {
	if (path.empty()) {
		pathData = GetProgramPath().GetPathStr();
		return *this;
	}
	char* start = path.data();
	char* end = start + path.size();
	//Not Absolute
	if (*start < 'A' || *start > 'Z' || start[1] != ':') {
		static Path selfPath = GetProgramPath();
		Path curPath = selfPath;
		vstd::vector<vstd::string> blocks;
		SeparatePath(path, blocks);
		for (auto ite = blocks.begin(); ite != blocks.end(); ++ite) {
			if (*ite == ".." || *ite == ".") {
				curPath = curPath.GetParentLevelPath();
			} else {
				curPath = curPath.GetSubLevelPath(*ite);
			}
		}
		pathData = curPath.pathData;
	}
	//Absolute Pos
	else {
		pathData = std::move(path);
	}
	start = pathData.data();
	end = start + pathData.size();
	for (char* i = start; i != end; ++i) {
		if (*i == '\\')
			*i = '/';
	}
	return *this;
}

bool Path::IsSubPathOf(Path const& parentPath) const {
	return parentPath.IsParentPathOf(*this);
}
bool Path::IsParentPathOf(Path const& subPath) const {
	auto&& otherPath = subPath.GetPathStr();
	vstd::vector<vstd::string> parentSplit;
	vstd::vector<vstd::string> subSplit;
	SeparatePath(pathData, parentSplit);
	SeparatePath(otherPath, subSplit);
	if (parentSplit.size() >= subSplit.size()) return false;
	size_t ss = 0;
	for (auto ite = parentSplit.begin(); ite != parentSplit.end(); ++ite) {
		if (*ite != subSplit[ss]) return false;
		ss++;
	}
	return true;
}
vstd::string Path::TryGetSubPath(Path const& subPath) const {
	auto&& otherPath = subPath.GetPathStr();
	vstd::vector<vstd::string> parentSplit;
	vstd::vector<vstd::string> subSplit;
	SeparatePath(pathData, parentSplit);
	SeparatePath(otherPath, subSplit);
	if (parentSplit.size() >= subSplit.size()) return vstd::string();
	auto ite = parentSplit.begin();
	size_t ss = 0;
	for (; ite != parentSplit.end(); ++ite) {
		if (*ite != subSplit[ss]) return vstd::string();
		ss++;
	}
	vstd::string str;
	for (size_t i = ss; i < subSplit.size() - 1; ++i) {
		str += subSplit[i] + '/';
	}
	str += subSplit[subSplit.size() - 1];
	return str;
}
Path& Path::operator=(Path const& v) {
	auto&& path = v.GetPathStr();
	return operator=(vstd::string(path));
}
void Path::SeparatePath(vstd::string_view path, vstd::vector<vstd::string>& blocks) {
	blocks.clear();
	char const* start = path.data();
	char const* end = start + path.size();
	char const* i;
	for (i = start; i < end; ++i) {
		if (*i == '\\' || *i == '/') {
			blocks.emplace_back(start, i);
			start = i + 1;
		}
	}
	if (start < end) {
		blocks.emplace_back(start, end);
	}
}
Path Path::GetProgramPath() {
	return Path(FileUtility::GetProgramPath());
}
Path Path::GetParentLevelPath() const {
	char const* start = pathData.data();
	char const* end = start + pathData.size();
	for (char const* i = end; i >= start; i--) {
		if (*i == '/' || *i == '\\') {
			return Path(vstd::string(start, i));
		}
	}
	return *this;
}
Path Path::GetSubLevelPath(vstd::string_view subName) const {
	vstd::string s;
	s.reserve(pathData.size() + subName.size() + 1);
	s << pathData << '/' << subName;
	return Path(std::move(s));
}
bool Path::IsFile() const {
	char const* start = pathData.data();
	char const* end = start + pathData.size();
	for (char const* i = end; i >= start; i--) {
		if (*i == '.')
			return true;
	}
	return false;
}
bool Path::IsDirectory() const {
	return !IsFile();
}

vstd::string Path::GetExtension() {
	char* start = pathData.data();
	char* end = start + pathData.size();
	for (char* i = end; i >= start; i--) {
		if (*i == '.')
			return vstd::string(i + 1, end);
	}
	return vstd::string();
}

void Path::TryCreateDirectory() {
	vstd::vector<uint> slashIndex;
	slashIndex.reserve(20);
	for (uint i = 0; i < pathData.length(); ++i) {
		if (pathData[i] == '/' || pathData[i] == '\\') {
			slashIndex.push_back(i);
			pathData[i] = '\\';
		}
	}
	if (!IsFile()) {
		slashIndex.push_back(pathData.length());
	}
	if (slashIndex.empty()) return;
	vstd::string command;
	command.reserve(slashIndex[slashIndex.size() - 1] + 3);
	uint startIndex = 0;
	for (uint i = 0; i < slashIndex.size(); ++i) {
		uint value = slashIndex[i];
		for (uint x = startIndex; x < value; ++x) {
			command += pathData[x];
		}
		if (_access(command.data(), 0) == -1) {
			std::system(("md " + command).data());
		}
		startIndex = slashIndex[i];
	}
}