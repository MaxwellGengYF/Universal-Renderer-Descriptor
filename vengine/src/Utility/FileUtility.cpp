
#include <Utility/FileUtility.h>
#include <io.h>
#include <stdio.h>
#include <direct.h>
#include <Utility/StringUtility.h>
#include <Utility/BinaryReader.h>
void FileUtility::GetFiles(vstd::string const& path, vstd::function<void(vstd::string&&)> const& func) {
	uint64 hFile = 0;
	struct _finddata_t fileinfo;
	if ((hFile = _findfirst((path + "/*").c_str(), &fileinfo)) != -1) {
		do {
			if ((fileinfo.attrib & _A_SUBDIR)) {
				if (strcmp(fileinfo.name, ".") != 0 && strcmp(fileinfo.name, "..") != 0) {
					GetFiles(path + ('/') + (fileinfo.name), func);
				}
			} else {
				func(path + '/' + (fileinfo.name));
			}
		} while (_findnext(hFile, &fileinfo) == 0);
		_findclose(hFile);
	}
}
void FileUtility::GetFolders(vstd::string const& path, vstd::function<void(vstd::string&&)> const& callback) {
	uint64 hFile = 0;
	struct _finddata_t fileinfo;
	if ((hFile = _findfirst((path + "/*").c_str(), &fileinfo)) != -1) {
		do {
			if ((fileinfo.attrib & _A_SUBDIR)) {
				if (strcmp(fileinfo.name, ".") != 0 && strcmp(fileinfo.name, "..") != 0) {
					callback(path + ('/') + (fileinfo.name));
				}
			}
		} while (_findnext(hFile, &fileinfo) == 0);
		_findclose(hFile);
	}
}

void FileUtility::GetFiles(vstd::string const& path, vstd::function<void(vstd::string&&)> const& func, vstd::function<bool(vstd::string const&, bool)> const& ignoreFileFunc) {
	uint64 hFile = 0;
	struct _finddata_t fileinfo;
	if ((hFile = _findfirst((path + "/*").c_str(), &fileinfo)) != -1) {
		do {
			if ((fileinfo.attrib & _A_SUBDIR)) {
				if (strcmp(fileinfo.name, ".") != 0 && strcmp(fileinfo.name, "..") != 0) {
					vstd::string str = path + ('/') + (fileinfo.name);
					if (ignoreFileFunc(str, false))
						GetFiles(str, func, ignoreFileFunc);
				}
			} else {
				vstd::string str = path + ('/') + (fileinfo.name);
				if (ignoreFileFunc(str, true))
					func(std::move(str));
			}
		} while (_findnext(hFile, &fileinfo) == 0);
		_findclose(hFile);
	}
}
void FileUtility::GetFiles(vstd::string const& path, vstd::vector<vstd::string>& files, vstd::HashMap<vstd::string, bool> const& ignoreFolders) {
	uint64 hFile = 0;
	struct _finddata_t fileinfo;

	if ((hFile = _findfirst((path + "/*").c_str(), &fileinfo)) != -1) {
		do {
			if ((fileinfo.attrib & _A_SUBDIR)) {
				if (ignoreFolders.Find(fileinfo.name)) continue;
				if (strcmp(fileinfo.name, ".") != 0 && strcmp(fileinfo.name, "..") != 0)
					GetFiles(path + ('/') + (fileinfo.name), files, ignoreFolders);
			} else {
				files.push_back(path + ('/') + (fileinfo.name));
			}
		} while (_findnext(hFile, &fileinfo) == 0);
		_findclose(hFile);
	}
}
void GetFileFixedExtense_Global_Func(vstd::string const& path, vstd::vector<vstd::string>& files, vstd::HashMap<vstd::string, bool> const& extense, vstd::string& cache) {
	uint64 hFile = 0;
	struct _finddata_t fileinfo;

	if ((hFile = _findfirst((path + "/*").c_str(), &fileinfo)) != -1) {
		do {
			if ((fileinfo.attrib & _A_SUBDIR)) {
				//if (ignoreFolders.Contains(fileinfo.name)) continue;
				if (strcmp(fileinfo.name, ".") != 0 && strcmp(fileinfo.name, "..") != 0) {
					vstd::string folderPath = path + ('/') + (fileinfo.name);
					//GetFiles(folderPath, files, folders, ignoreFolders);
					GetFileFixedExtense_Global_Func(folderPath, files, extense, cache);
				}
			} else {
				char const* startPtr = fileinfo.name;
				char const* ptr;
				for (ptr = fileinfo.name; *ptr != 0; ++ptr) {
					if (*ptr == '.') {
						startPtr = ptr + 1;
					}
				}
				cache.clear();
				cache << vstd::string_view(startPtr, ptr - startPtr);
				auto ite = extense.Find(cache);
				if (ite && ite.Value()) {
					files.push_back(path + ('/') + (fileinfo.name));
				}
			}
		} while (_findnext(hFile, &fileinfo) == 0);
		_findclose(hFile);
	}
}
void FileUtility::GetFilesFixedExtense(vstd::string const& originPath, vstd::vector<vstd::string>& files, vstd::HashMap<vstd::string, bool> const& extense) {
	vstd::string cache;
	GetFileFixedExtense_Global_Func(originPath, files, extense, cache);
}

vstd::string_view FileUtility::GetFileExtension(vstd::string const& filePath) {
	char const* ptr = filePath.data() + filePath.size() - 1;
	for (; ptr >= filePath.data(); --ptr) {
		if (*ptr == '.') {
			return vstd::string_view(ptr, filePath.data() + filePath.size());
		}
	}
	return vstd::string_view();
}

vstd::string_view FileUtility::GetFileName(vstd::string const& filePath) {
	char const* ptr = filePath.data() + filePath.size() - 1;
	for (; ptr >= filePath.data(); --ptr) {
		if (*ptr == '/' || *ptr == '\\') {
			return vstd::string_view(ptr, filePath.data() + filePath.size());
		}
	}
	return filePath;
}
void FileUtility::ToLegalFolderPath(vstd::string& path) {
	if (path.empty())return;
	for (auto&& i : path) {
		if (i == '\\') i = '/';
	}
	auto&& last = *(path.end() - 1);
	if (last != '/') path += '/';
}

void FileUtility::GetFiles(vstd::string const& path, vstd::vector<vstd::string>& files, vstd::vector<vstd::string>& folders, vstd::HashMap<vstd::string, bool> const& ignoreFolders) {
	uint64 hFile = 0;
	struct _finddata_t fileinfo;

	if ((hFile = _findfirst((path + "/*").c_str(), &fileinfo)) != -1) {
		do {
			if ((fileinfo.attrib & _A_SUBDIR)) {
				if (ignoreFolders.Find(fileinfo.name)) continue;
				if (strcmp(fileinfo.name, ".") != 0 && strcmp(fileinfo.name, "..") != 0) {
					vstd::string& folderPath = folders.emplace_back(path + ('/') + (fileinfo.name));
					GetFiles(folderPath, files, folders, ignoreFolders);
				}
			} else {
				files.push_back(path + ('/') + (fileinfo.name));
			}
		} while (_findnext(hFile, &fileinfo) == 0);
		_findclose(hFile);
	}
}

bool FileUtility::ReadCommandFile(vstd::string const& path, vstd::HashMap<vstd::string, vstd::function<void(vstd::string const&)>>& rnb) {
	vstd::string name;
	vstd::vector<vstd::string_view> lines;
	{
		BinaryReader rd(path);
		if (!rd) return false;
		name.resize(rd.GetLength());
		rd.Read(name.data(), name.size());
	}

	//TODO
	//Add Command
	vstd::vector<vstd::string_view> commands;
	for (auto ite = lines.begin(); ite != lines.end(); ++ite) {
		
		auto splitIte = StringUtil::Split(*ite, ' ');
		for (auto&& i : splitIte) {
			commands.push_back(i);
		}
		if (commands.size() < 1) continue;
		auto hashIte = rnb.Find(commands[0]);
		if (hashIte) {
			if (commands.size() >= 2) {
				vstd::string str;
				for (uint a = 1; a < commands.size() - 1; ++a) {
					str += commands[a];
					str += ' ';
				}
				str += commands[commands.size() - 1];
				hashIte.Value()(str);
			} else {
				hashIte.Value()(vstd::string());
			}
		}
	}
	return true;
}
void FileUtility::GetTrivialFiles(vstd::string const& path, vstd::function<void(vstd::string&&)> const& callback) {
	uint64 hFile = 0;
	struct _finddata_t fileinfo;

	if ((hFile = _findfirst((path + "/*").c_str(), &fileinfo)) != -1) {
		do {
			if (!(fileinfo.attrib & _A_SUBDIR)) {
				callback(fileinfo.name);
			}
		} while (_findnext(hFile, &fileinfo) == 0);
		_findclose(hFile);
	}
}

vstd::string FileUtility::GetProgramPath() {
	vstd::string str;
	str.resize(4096);
	_getcwd(str.data(), str.size());
	str.resize(strlen(str.data()));
	return str;
}

void FileUtility::GetFolders(vstd::vector<vstd::string>& files) {
	auto str = GetProgramPath();
	str += "/*";
	uint64 hFile = 0;
	struct _finddata_t fileinfo;

	if ((hFile = _findfirst(str.c_str(), &fileinfo)) != -1) {
		do {
			if ((fileinfo.attrib & _A_SUBDIR)) {
				if (fileinfo.name[0] != '.')
					files.push_back(fileinfo.name);
			}
		} while (_findnext(hFile, &fileinfo) == 0);
		_findclose(hFile);
	}
}