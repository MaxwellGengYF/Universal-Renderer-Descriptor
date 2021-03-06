#pragma once
#include <Common/Common.h>
#include <Windows.h>
#include <Graphics/d3dx12.h>
#include <DirectX/Shader/ShaderVariableType.h>
#include <DirectX/Resource/Buffer.h>
#include <DirectX/Resource/DescriptorHeap.h>
namespace toolhub::directx {
using Microsoft::WRL::ComPtr;
class TopAccel;
class CommandBufferBuilder;
class Shader : public vstd::IOperatorNewBase {
public:
	struct Property {
		ShaderVariableType type;
		uint spaceIndex;
		uint registerIndex;
		uint arrSize;
	};
	enum class Tag : vbyte {
		ComputeShader,
		RayTracingShader
	};
	virtual Tag GetTag() const = 0;

protected:
	struct InsideProperty : public Property {
		uint rootSigPos;
		InsideProperty(Property const& p)
			: Property(p) {}
	};
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSig;
	vstd::HashMap<vstd::string, InsideProperty> properties;
	vstd::optional<InsideProperty> GetProperty(vstd::string_view str) const;

public:
	Shader(
		vstd::span<std::pair<vstd::string, Property> const> properties,
		ID3D12Device* device);
	Shader(
		vstd::span<std::pair<vstd::string, Property> const> properties,
		ComPtr<ID3D12RootSignature>&& rootSig);
	Shader(Shader&& v) = default;
	ID3D12RootSignature* RootSig() const { return rootSig.Get(); }

	bool SetComputeResource(
		vstd::string_view propertyName,
		CommandBufferBuilder* cmdList,
		BufferView buffer) const;
	bool SetComputeResource(
		vstd::string_view propertyName,
		CommandBufferBuilder* cmdList,
		DescriptorHeapView view) const;
	bool SetComputeResource(
		vstd::string_view propertyName,
		CommandBufferBuilder* cmdList,
		TopAccel const* bAccel) const;

	KILL_COPY_CONSTRUCT(Shader)
	virtual ~Shader() = default;
};
}// namespace toolhub::directx