#pragma once
#include <Graphics/d3dx12.h>
namespace toolhub::directx {
class GlobalSamplers {
public:
    static vstd::span<D3D12_STATIC_SAMPLER_DESC> GetSamplers();
};
}// namespace toolhub::directx