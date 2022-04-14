
#include <Graphics/SceneDescriptor/IRTShader.h>
#include <Graphics/IGraphicsFactory.h>
namespace toolhub::graphics {
class RTShader : public IRTShader {
public:
	Tag GetTag() const override { return Tag::RTShader; }
	RTShader() {
	}
	RTShader(
		db::IJsonDict const* dict,
		vstd::span<vbyte const> serData) {
		SetBase(dict);
		ReadProperty(dict);
		ReadBin(serData);
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
		dict->Set("payload", payloadSize);
		dict->Set("recursive", maxRecursiveCount);
		if (closestHit)
			dict->Set("closest", true);
		if (anyHit)
			dict->Set("any", true);
		if (intersect)
			dict->Set("intersect", true);
		SaveBase(dict.get());
		return dict;
	}
	void DeSerialize(db::IJsonDict const* dict) {
		SetBase(dict);
		ReadProperty(dict);
		payloadSize = dict->Get("payload").template get_or<int64>(1);
		maxRecursiveCount = dict->Get("recursive").template get_or<int64>(0);
		closestHit = dict->Get("closest").template get_or<bool>(false);
		intersect = dict->Get("intersect").template get_or<bool>(false);
		anyHit = dict->Get("any").template get_or<bool>(false);
	}
	VSTD_SELF_PTR
};
IResource* GraphicsFactory_Impl::CreateRayTracing() const {
	return new RTShader();
}
IResource* GraphicsFactory_Impl::CreateRayTracing(
	db::IJsonDict const* obj,
	vstd::span<vbyte const> serData) const {
	return new RTShader(obj, serData);
}
}// namespace toolhub::graphics