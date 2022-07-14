#pragma once
#include <Common/Common.h>
#include <Common/functional.h>
#include <io.h>
class VENGINE_DLL_COMMON FileUtility {
private:
	FileUtility() = delete;
	~FileUtility() = delete;

public:
	static bool ReadCommandFile(vstd::string const& path, vstd::HashMap<vstd::string, vstd::function<void(vstd::string const&)>>& rnb);
	static void GetFiles(vstd::string const& path, vstd::vector<vstd::string>& files, vstd::HashMap<vstd::string, bool> const& ignoreFolders);
	static void GetFiles(vstd::string const& path, vstd::function<void(vstd::string&&)> const& func);
	static void GetFiles(
		vstd::string const& path,
		vstd::function<void(vstd::string&&)> const& func,					   //execute func(path)
		vstd::function<bool(vstd::string const&, bool)> const& ignoreFileFunc);//ignore func(path, isFile)
	static void GetFilesFixedExtense(vstd::string const& path, vstd::vector<vstd::string>& files, vstd::HashMap<vstd::string, bool> const& extense);
	static void GetFiles(vstd::string const& path, vstd::vector<vstd::string>& files, vstd::vector<vstd::string>& folders, vstd::HashMap<vstd::string, bool> const& ignoreFolders);
	static void GetFolders(vstd::vector<vstd::string>& files);
	static void GetTrivialFiles(vstd::string const& path, vstd::function<void(vstd::string&&)> const& callback);
	static void GetFolders(vstd::string const& path, vstd::function<void(vstd::string&&)> const& callback);
	static vstd::string GetProgramPath();
	static vstd::string_view GetFileExtension(vstd::string const& filePath);//.xxx
	static vstd::string_view GetFileName(vstd::string const& filePath);		//.xxx
	static void ToLegalFolderPath(vstd::string& path);
};