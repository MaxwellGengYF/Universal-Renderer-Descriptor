
#include <Graphics/SceneDescriptor/IMesh.h>
#include <Graphics/IGraphicsFactory.h>
namespace toolhub::graphics {
class Mesh final : public IMesh {
public:
	vstd::span<float3 const> position;
	vstd::span<float3 const> normal;
	vstd::span<float4 const> tangent;
	vstd::span<float4 const> color;
	vstd::span<uint const> indices;
	vstd::vector<vstd::span<float2 const>> uvs;
	Tag GetTag() const override { return Tag::Mesh; }

	void UpdateVertices(
		vstd::span<float3 const> position,
		vstd::span<float3 const> normal,
		vstd::span<float4 const> tangent,
		vstd::span<float4 const> color,
		vstd::span<vstd::span<float2 const>> uvs) override {
		this->position = position;
		this->normal = normal;
		this->tangent = tangent;
		this->color = color;
		this->uvs.clear();
		this->uvs.push_back_all(uvs);
	}
	Vertices GetVertices() const override {
		return {
			position,
			normal,
			tangent,
			color,
			vstd::span<vstd::span<float2 const> const>(uvs)};
	}
	template<typename T>
	static vstd::span<T const> GetRange(
		vstd::span<vbyte const>& binData,
		int64 eleCount) {
		vstd::span<T const> result{
			reinterpret_cast<T const*>(binData.data()),
			size_t(eleCount)};
		size_t byteSize = eleCount * sizeof(T);
		binData = {
			binData.data() + byteSize,
			binData.size() - byteSize};
		return result;
	}
	template<typename T>
	static vstd::span<T const> GetRange(vstd::string_view name, db::IJsonDict const* dict, vstd::span<vbyte const>& binData) {
		auto v = dict->Get(name).template get_or<int64>(0);
		return GetRange<T>(binData, v);
	}
	Mesh() {}
	Mesh(
		db::IJsonDict const* dict,
		vstd::span<vbyte const> serData) {
		SetBase(dict);
#define GET_RANGE(x) x = GetRange<typename decltype(x)::element_type>(#x##_sv, dict, serData)
		GET_RANGE(position);
		GET_RANGE(normal);
		GET_RANGE(tangent);
		GET_RANGE(color);
		GET_RANGE(indices);
#undef GET_RANGE
		auto uvArr = dict->Get("uv").template get_or<db::IJsonArray*>(nullptr);
		if (uvArr) {
			uvs.reserve(uvArr->Length());
			for (auto&& i : *uvArr) {
				auto eleCount = i.template get_or<int64>(0);
				uvs.emplace_back(GetRange<float2>(serData, eleCount));
			}
		}
	}
	~Mesh() {}
	template<typename T>
	static void PushDataToVector(vstd::span<T const> sp, vstd::vector<vbyte>& data) {
		data.push_back_all(
			reinterpret_cast<vbyte const*>(sp.data()),
			sp.size() * sizeof(T));
	}

	void Save(vstd::vector<vbyte>& data, db::IJsonDatabase* db) override {
		auto dict = db->CreateDict();
#define SET_RANGE(x) \
	if (!x.empty()) dict->Set(#x##_sv, x.size())
		SET_RANGE(position);
		SET_RANGE(normal);
		SET_RANGE(tangent);
		SET_RANGE(color);
		SET_RANGE(indices);
#undef SET_RANGE
		auto uvArr = db->CreateArray();
		uvArr->Reserve(uvs.size());
		for (auto&& i : uvs) {
			uvArr->Add(i.size());
		}
		dict->Set("uv"_sv, std::move(uvArr));
		SaveBase(dict.get());
		dict->Serialize(data);
		PushDataToVector(position, data);
		PushDataToVector(normal, data);
		PushDataToVector(tangent, data);
		PushDataToVector(color, data);
		PushDataToVector(indices, data);
		for (auto&& i : uvs) {
			PushDataToVector(i, data);
		}
	}
	vstd::span<uint const> GetIndices() const override {
		return indices;
	}
	void UpdateIndices(vstd::span<uint const> indices) override {
		this->indices = indices;
	}
	VSTD_SELF_PTR
};
IResource* GraphicsFactory_Impl::CreateMesh() const {
	return new Mesh();
}
IResource* GraphicsFactory_Impl::CreateMesh(
	db::IJsonDict const* obj,
	vstd::span<vbyte const> serData) const {
	return new Mesh(obj, serData);
}
}// namespace toolhub::graphics