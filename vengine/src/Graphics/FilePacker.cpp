
#include <Graphics/FilePacker.h>
#include <Network/FunctionSerializer.h>
namespace toolhub {
bool FilePacker::Save(
	db::IJsonObject* serObj,
	vstd::string const& path,
	vstd::span<vbyte const> binData) {
	auto f = fopen(path.c_str(), "wb");
	if (!f) return false;
	auto dsp = vstd::create_disposer([&] {
		fclose(f);
	});

	vstd::vector<vbyte> data;
	data.resize(sizeof(uint64) + sizeof(PrimLoadTag));
	size_t lastSize = data.size();
	if (serObj)
		serObj->Serialize(data);
	*reinterpret_cast<size_t*>(data.data()) = data.size() - lastSize;
	*reinterpret_cast<PrimLoadTag*>(data.data() + sizeof(uint64)) =
		serObj
			? (serObj->IsDict() ? PrimLoadTag::Dict : PrimLoadTag::Array)
			: PrimLoadTag::None;
	fwrite(data.data(), data.size(), 1, f);
	fwrite(binData.data(), binData.size(), 1, f);
	return true;
}
void FilePacker::Save(
	vstd::vector<vbyte>& data,
	db::IJsonObject* serObj,
	vstd::span<vbyte const> binData) {
	auto oriSize = data.size();
	constexpr auto headerSize = sizeof(uint64) + sizeof(PrimLoadTag);
	data.reserve(oriSize + headerSize + binData.size());
	data.resize(oriSize + headerSize);
	size_t lastSize = data.size();
	if (serObj)
		serObj->Serialize(data);
	*reinterpret_cast<uint64*>(data.data() + oriSize) = data.size() - lastSize;
	*reinterpret_cast<PrimLoadTag*>(data.data() + oriSize + sizeof(uint64)) =
		serObj
			? (serObj->IsDict() ? PrimLoadTag::Dict : PrimLoadTag::Array)
			: PrimLoadTag::None;
	lastSize = data.size();
	data.resize(lastSize + binData.size());
	memcpy(data.data() + lastSize, binData.data(), binData.size());
}
std::pair<
	vstd::unique_ptr<db::IJsonObject>,
	vstd::span<vbyte const>>
FilePacker::Load(db::IJsonDatabase* db,
				 vstd::span<const vbyte> sp) {
	std::pair<
		vstd::unique_ptr<db::IJsonObject>,
		vstd::span<vbyte const>>
		result;
	auto serSize = vstd::SerDe<uint64>::Get(sp);
	auto tag = vstd::SerDe<PrimLoadTag>::Get(sp);
	if (serSize > 0) {
		auto setSp = [&] {
			sp = {sp.data() + serSize, sp.size() - serSize};
		};
		if (tag == PrimLoadTag::Dict) {
			result.first = vstd::unique_ptr<db::IJsonObject>(db->CreateDict_RawPtr());
			result.first->Read({sp.data(), serSize}, false);
			setSp();
		} else if (tag == PrimLoadTag::Array) {
			result.first = vstd::unique_ptr<db::IJsonObject>(db->CreateArray_RawPtr());
			result.first->Read({sp.data(), serSize}, false);
			setSp();
		}
		result.second = sp;
	}
	return result;
}
FilePacker::FilePacker(
	db::IJsonDatabase* db,
	vstd::span<const vbyte> sp) {
	auto result = Load(db, sp);
	obj = std::move(result.first);
	binBytes.resize(result.second.size());
	memcpy(binBytes.data(), result.second.data(), result.second.size());
}
FilePacker::FilePacker(
	db::IJsonDatabase* db,
	vstd::string const& path) {
	auto f = fopen(path.c_str(), "rb");
	if (!f) return;
	auto dsp = vstd::create_disposer([&] {
		fclose(f);
	});
	vstd::vector<vbyte> vec;
	vec.resize(sizeof(uint64) + sizeof(PrimLoadTag));
	size_t offset = vec.size();
	fread(vec.data(), vec.size(), 1, f);
	vstd::span<vbyte const> sp = vec;
	auto serSize = vstd::SerDe<uint64>::Get(sp);
	auto tag = vstd::SerDe<PrimLoadTag>::Get(sp);
	if (serSize > 0) {
		vec.clear();
		vec.resize(serSize);
		fread(vec.data(), vec.size(), 1, f);
		offset += serSize;
		if (tag == PrimLoadTag::Dict) {
			obj = vstd::unique_ptr<db::IJsonObject>(db->CreateDict_RawPtr());
			obj->Read(vec, false);
		} else if (tag == PrimLoadTag::Array) {
			obj = vstd::unique_ptr<db::IJsonObject>(db->CreateArray_RawPtr());
			obj->Read(vec, false);
		}
	}

	fseek(f, 0, SEEK_END);
	auto binLen = ftell(f);
	fseek(f, offset, SEEK_SET);
	binLen -= offset;
	binBytes.resize(binLen);
	fread(binBytes.data(), binLen, 1, f);
	loaded = true;
}
}// namespace toolhub