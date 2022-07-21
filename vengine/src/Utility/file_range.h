#pragma once
#include <Common/Common.h>
namespace vstd {
class VENGINE_DLL_COMMON FileRecursiveRange : public detail::RangeFlag {
public:
	enum class LoadNodeState : uint {
		Continue,
		SubFolder,
		File
	};
	struct Node {
		vstd::string path;
		uint64 hFile;
		Node(vstd::string&& path);
		LoadNodeState Init(vstd::vector<Node>& vec, vstd::string& str);
		LoadNodeState Next(vstd::vector<Node>& vec, vstd::string& str);
	};

private:
	vstd::string root;
	vstd::string str;
	vstd::vector<Node> stack;
	void FindNext(LoadNodeState& state);

public:
	FileRecursiveRange(vstd::string rootPath);
	IteRef<FileRecursiveRange> begin();
	void operator++();
	bool operator==(IteEndTag) const;
	vstd::IteEndTag end() { return {}; }
	~FileRecursiveRange();
	vstd::string const& operator*() { return str; }
};
class VENGINE_DLL_COMMON FileRange : public detail::RangeFlag {
private:
	vstd::string root;
	vstd::string str;
	uint64 hFile;

public:
	FileRange(vstd::string rootPath);
	IteRef<FileRange> begin();
	void operator++();
	bool operator==(IteEndTag) const;
	vstd::IteEndTag end() { return {}; }
	~FileRange();
	vstd::string const& operator*() { return str; }
};
}// namespace vstd