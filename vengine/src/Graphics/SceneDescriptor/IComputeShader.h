#pragma once
#include <Graphics/SceneDescriptor/IShader.h>
namespace toolhub::graphics {
class IComputeShader : public IShader {
public:
	uint3 blockSize = {1,1,1};
	virtual ~IComputeShader() = default;
};
}// namespace toolhub::graphics