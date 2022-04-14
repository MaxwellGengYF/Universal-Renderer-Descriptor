#pragma once
#include <Common/Common.h>
#include <Graphics/SceneDescriptor/IShader.h>
namespace toolhub::graphics {
class IShaderCompiler;
class IResource;
class IGraphicsFactory {
public:
	virtual IShaderCompiler* CreateDirectXCompiler() const = 0;
	virtual IResource* CreateResource(
		db::IJsonDatabase* db,
		vstd::span<vbyte const> serData) const = 0;
	virtual IResource* CreateResource(
		IResource::Tag tag) const = 0;
};
#ifdef VENGINE_GRAPHICS_PROJECT
class IComputeShader;
VENGINE_UNITY_EXTERN IGraphicsFactory const* Graphics_GetFactory();
class GraphicsFactory_Impl : public IGraphicsFactory {
public:
	IShaderCompiler* CreateDirectXCompiler() const override;
	IResource* CreateResource(
		db::IJsonDatabase* db,
		vstd::span<vbyte const> serData) const override;
	IResource* CreateResource(
		IResource::Tag tag) const override;
	IResource* CreateCompute() const;
	IResource* CreateMesh() const;
	IResource* CreateMesh(
		db::IJsonDict const* obj,
		vstd::span<vbyte const> serData) const;
	IResource* CreateCompute(
		db::IJsonDict const* obj,
		vstd::span<vbyte const> serData) const;
	IResource* CreateRayTracing() const;
	IResource* CreateRayTracing(
		db::IJsonDict const* obj,
		vstd::span<vbyte const> serData) const;
};
#endif
}// namespace toolhub::graphics