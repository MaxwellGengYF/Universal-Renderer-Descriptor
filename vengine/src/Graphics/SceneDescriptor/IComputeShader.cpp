
#include <Graphics/SceneDescriptor/IComputeShader.h>
#include <Graphics/IGraphicsFactory.h>
namespace toolhub::graphics {
class ComputeShader final : public IComputeShader {
public:
	Tag GetTag() const override { return Tag::ComputeShader; }
	ComputeShader() {
	}
	ComputeShader(
		db::IJsonDict const* dict,
		vstd::span<vbyte const> serData) {
		ReadBin(serData);
		DeSerialize(dict);
		//TODO: bin
	}
	void Save(vstd::vector<vbyte>& data, db::IJsonDatabase* db) override {
		auto dict = Serialize(db);
		FilePacker::Save(
			data,
			dict.get(),
			binBytes.multi_visit_or(
				vstd::span<vbyte const>(static_cast<vbyte const*>(nullptr), size_t(0)),
				[&](auto&& b) -> vstd::span<vbyte const> {
					return {static_cast<vbyte const*>(b->GetBufferPtr()), b->GetBufferSize()};
				},
				[&](auto&& b) -> vstd::span<vbyte const> {
					return b;
				}));
		//TODO: save & bin
	}
	vstd::unique_ptr<db::IJsonDict> Serialize(db::IJsonDatabase* db) const {
		auto dict = SaveProperty(db);
		SaveBase(dict.get());
		auto arr = db->CreateArray();
		*arr << blockSize.x << blockSize.y << blockSize.z;
		dict->Set("block", std::move(arr));
		return dict;
	}
	void DeSerialize(db::IJsonDict const* dict) {
		SetBase(dict);
		ReadProperty(dict);
		auto blkArr = dict->Get("block").template get_or<db::IJsonArray*>(nullptr);
		if (blkArr && blkArr->Length() >= 3) {
			blockSize.x = (*blkArr)[0].template get_or<int64>(1);
			blockSize.y = (*blkArr)[1].template get_or<int64>(1);
			blockSize.z = (*blkArr)[2].template get_or<int64>(1);
		} else {
			blockSize = {1, 1, 1};
		}
	}
	VSTD_SELF_PTR
};
IResource* GraphicsFactory_Impl::CreateCompute(
	db::IJsonDict const* obj,
	vstd::span<vbyte const> serData) const {
	return new ComputeShader(obj, serData);
}
IResource* GraphicsFactory_Impl::CreateCompute() const {
	return new ComputeShader();
}

}// namespace toolhub::graphics