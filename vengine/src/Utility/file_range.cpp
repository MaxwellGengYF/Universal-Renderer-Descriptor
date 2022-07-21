#include "file_range.h"
#include <io.h>
#include <Common/string_builder.h>
namespace vstd {
FileRecursiveRange::Node::Node(vstd::string&& path)
	: path(std::move(path)) {
}
namespace detail {
static void LoadFileName(_finddata_t& fileinfo, vstd::string const& path, vstd::string& str) {
	size_t size = strlen(fileinfo.name);
	vstd::string_view fileName(fileinfo.name, size);
	str.reserve(path.size() + fileName.size() + 1);
	StringBuilder(&str) << path << '/' << fileName;
}
static FileRecursiveRange::LoadNodeState FileInfoToString(_finddata_t& fileinfo, vstd::string const& path, vstd::vector<FileRecursiveRange::Node>& vec, vstd::string& str) {
	if ((fileinfo.attrib & _A_SUBDIR)) {
		size_t size = strlen(fileinfo.name);
		vstd::string_view fileName(fileinfo.name, size);
		if (fileName != "."_sv && fileName != ".."_sv) {
			vstd::string newValue;
			newValue.reserve(path.size() + fileName.size() + 1);
			StringBuilder(&newValue) << path << '/' << fileName;
			vec.emplace_back(std::move(newValue));
			return FileRecursiveRange::LoadNodeState::SubFolder;
		} else {
			return FileRecursiveRange::LoadNodeState::Continue;
		}
	} else {
		str.clear();
		LoadFileName(fileinfo, path, str);
		return FileRecursiveRange::LoadNodeState::File;
	}
}
}// namespace detail

FileRecursiveRange::LoadNodeState FileRecursiveRange::Node::Init(vstd::vector<Node>& vec, vstd::string& str) {
	_finddata_t fileinfo;
	hFile = _findfirst((path + "/*").c_str(), &fileinfo);
	if (hFile == -1) {
		auto disp = vstd::create_disposer([&] { vec.erase_last(); });
		return LoadNodeState::Continue;
	}
	return detail::FileInfoToString(fileinfo, path, vec, str);
}
FileRecursiveRange::LoadNodeState FileRecursiveRange::Node::Next(vstd::vector<Node>& vec, vstd::string& str) {
	_finddata_t fileinfo;
	if (_findnext(hFile, &fileinfo) != 0) {
		auto disp = vstd::create_disposer([&] { vec.erase_last(); });
		return LoadNodeState::Continue;
	}
	return detail::FileInfoToString(fileinfo, path, vec, str);
}

FileRecursiveRange::FileRecursiveRange(vstd::string rootPath)
	: root(std::move(rootPath)) {
}
void FileRecursiveRange::FindNext(LoadNodeState& state) {
	do {
		auto value = stack.last();
		switch (state) {
			case LoadNodeState::SubFolder: {
				state = value->Init(stack, str);
			} break;
			case LoadNodeState::Continue: {
				state = value->Next(stack, str);
			} break;
			case LoadNodeState::File: {
				return;
			} break;
		}
	} while (!stack.empty());
}

IteRef<FileRecursiveRange> FileRecursiveRange::begin() {
	LoadNodeState state = LoadNodeState::SubFolder;
	stack.clear();
	stack.emplace_back(vstd::string(root));
	FindNext(state);
	return {this};
}
void FileRecursiveRange::operator++() {
	LoadNodeState state = LoadNodeState::Continue;
	FindNext(state);
}
bool FileRecursiveRange::operator==(IteEndTag) const {
	return stack.empty();
}
FileRecursiveRange::~FileRecursiveRange() {}

FileRange::FileRange(vstd::string rootPath)
	: root(std::move(rootPath)) {}
IteRef<FileRange> FileRange::begin() {
	str.clear();
	_finddata_t fileinfo;
	hFile = _findfirst((root + "/*").c_str(), &fileinfo);
	if (hFile == -1) {
		return {this};
	}
	if (!(fileinfo.attrib & _A_SUBDIR)) {
		detail::LoadFileName(fileinfo, root, str);
		return {this};
	}
	do {
		if (!(fileinfo.attrib & _A_SUBDIR)) {
			detail::LoadFileName(fileinfo, root, str);
			return {this};
		}
	} while (_findnext(hFile, &fileinfo) == 0);
	return {this};
}
bool FileRange::operator==(IteEndTag) const {
	return str.empty();
}
void FileRange::operator++() {
	str.clear();
	_finddata_t fileinfo;
	while (_findnext(hFile, &fileinfo) == 0) {
		if (!(fileinfo.attrib & _A_SUBDIR)) {
			detail::LoadFileName(fileinfo, root, str);
		}
	};
}
FileRange::~FileRange() {}
}// namespace vstd