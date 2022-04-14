#pragma once
#include <Graphics/SceneDescriptor/IShader.h>
namespace toolhub::graphics {
class IRTShader : public IShader{
public:
	size_t payloadSize = 16;
	size_t maxRecursiveCount = 1;
	bool closestHit = false;
	bool anyHit = false;
	bool intersect = false;
	virtual ~IRTShader() = default;
};
}// namespace toolhub::graphics