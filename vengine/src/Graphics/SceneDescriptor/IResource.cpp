
#include <Graphics/SceneDescriptor/IResource.h>
#include <Graphics/IGraphicsFactory.h>
#include <Graphics/SceneDescriptor/IComputeShader.h>
#include <Graphics/SceneDescriptor/IMesh.h>
namespace toolhub::graphics {
void IResource::SaveBase(db::IJsonDict* dict) const {
	dict->Set("guid", *guid);
	dict->Set("tag", int64(GetTag()));
}
void IResource::SetBase(db::IJsonDict const* dict) {
	guid = dict->Get("guid").template try_get<vstd::Guid>();
}

void IResource::CreateGuid() {
	guid.New(true);
}

IResource* GraphicsFactory_Impl::CreateResource(
	db::IJsonDatabase* db,
	vstd::span<vbyte const> serData) const {
	auto serResult = FilePacker::Load(db, serData);
	auto&& obj = serResult.first.get();
	if (!obj->IsDict()) return nullptr;
	auto dict = static_cast<db::IJsonDict*>(obj);
	auto t = dict->Get("tag").template try_get<int64>();
	if (!t) return nullptr;
	switch (static_cast<IResource::Tag>(*t)) {
		case IResource::Tag::ComputeShader:
			return CreateCompute(
				dict,
				serResult.second);
		case IResource::Tag::RTShader:
			return CreateRayTracing(
				dict,
				serResult.second);
		case IResource::Tag::Mesh:
			return CreateMesh(
				dict,
				serResult.second);
		default:
			return nullptr;
	}
}
IResource* GraphicsFactory_Impl::CreateResource(
	IResource::Tag tag) const {
	switch (tag) {
		case IResource::Tag::ComputeShader:
			return CreateCompute();
		case IResource::Tag::RTShader:
			return CreateRayTracing();
		case IResource::Tag::Mesh:
			return CreateMesh();
		default:
			return nullptr;
	}
}
}// namespace toolhub::graphics