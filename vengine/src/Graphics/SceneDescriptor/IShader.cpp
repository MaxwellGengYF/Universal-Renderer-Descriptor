
#include <Graphics/SceneDescriptor/IShader.h>
namespace toolhub::graphics {
vstd::unique_ptr<db::IJsonDict> IShader::SaveProperty(db::IJsonDatabase* db) const {
	auto dct = db->CreateDict();
	auto arr = db->CreateArray();
	arr->Reserve(properties.size());
	for (auto&& i : properties) {
		auto s = db->CreateArray();
		s->Reserve(3);
		s->Add(i.name);
		s->Add(i.arrSize);
		s->Add(int64(i.type));
		arr->Add(std::move(s));
	}
	dct->Set("property", std::move(arr));
	dct->Set("sm", shaderModel);
	return dct;
}
void IShader::ReadProperty(db::IJsonDict const* dct) {
	auto arr = dct->Get("property").template get_or<db::IJsonArray*>(nullptr);
	if (arr) {
		properties.clear();
		properties.reserve(arr->Length());
		for (auto&& i : *arr) {
			auto a = i.template get_or<db::IJsonArray*>(nullptr);
			if (!a) continue;
			if (a->Length() < 3) continue;
			auto name = (*a)[0].template try_get<vstd::string_view>();
			auto sz = (*a)[1].template try_get<int64>();
			auto tp = (*a)[2].template try_get<int64>();
			if (name && sz && tp) {
				properties.emplace_back(Property{
					vstd::string(*name),
					uint64(*sz),
					static_cast<PropertyType>(*tp)});
			}
		}
	}
	shaderModel = dct->Get("sm").template get_or<int64>(0);
	
}
void IShader::ReadBin(vstd::span<vbyte const> bin) {
	binBytes = bin;
}

}// namespace toolhub::graphics