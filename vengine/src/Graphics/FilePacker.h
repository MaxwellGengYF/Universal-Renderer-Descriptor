#pragma once
#include <Database/IJsonObject.h>
namespace toolhub {
class FilePacker {
private:
	bool loaded = false;

public:
	enum class PrimLoadTag : vbyte {
		None,
		Array,
		Dict
	};
	vstd::unique_ptr<db::IJsonObject> obj;
	vstd::vector<vbyte, VEngine_AllocType::VEngine, 32> binBytes;
	static bool Save(
		db::IJsonObject* serObj,
		vstd::string const& path,
		vstd::span<vbyte const> binData);
	static void Save(
		vstd::vector<vbyte>& vec,
		db::IJsonObject* serObj,
		vstd::span<vbyte const> binData);
	FilePacker(
		db::IJsonDatabase* db,
		vstd::string const& path);
	FilePacker(
		db::IJsonDatabase* db,
		vstd::span<const vbyte> bytes);
	static std::pair<
		vstd::unique_ptr<db::IJsonObject>,
		vstd::span<vbyte const>>
	Load(db::IJsonDatabase* db,
		 vstd::span<const vbyte> bytes);
	bool Loaded() const { return loaded; }
};
}// namespace toolhub