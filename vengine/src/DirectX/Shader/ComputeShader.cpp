
#include <DirectX/Shader/ComputeShader.h>
namespace toolhub::directx {
ComputeShader::ComputeShader(
	vstd::span<std::pair<vstd::string, Property> const>&& properties,
	vstd::span<vbyte> binData,
	ID3D12Device* device)
	: Shader(std::move(properties), device) {

	D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.pRootSignature = rootSig.Get();
	psoDesc.CS.pShaderBytecode = binData.data();
	psoDesc.CS.BytecodeLength = binData.size();
	psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	ThrowIfFailed(device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(pso.GetAddressOf())));
}
ComputeShader::ComputeShader(
	vstd::span<std::pair<vstd::string, Property> const> prop,
	ComPtr<ID3D12RootSignature>&& rootSig,
	vstd::span<vbyte const> code,
	ID3D12Device* device)
	: Shader(prop, std::move(rootSig)) {
	D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.pRootSignature = this->rootSig.Get();
	psoDesc.CS.pShaderBytecode = code.data();
	psoDesc.CS.BytecodeLength = code.size();
	psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	ThrowIfFailed(device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(pso.GetAddressOf())));
}
ComputeShader ::~ComputeShader() {
}
}// namespace toolhub::directx