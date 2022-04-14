#pragma once
#include <Common/Common.h>
namespace toolhub::directx {
struct MeshInstance {
	uint vbIdx;
	uint ibIdx;
	uint vertStride;
	uint vertCount;
	uint idxCount;
};
}// namespace toolhub::directx