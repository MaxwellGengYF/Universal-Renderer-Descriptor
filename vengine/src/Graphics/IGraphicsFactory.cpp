
#include <Graphics/IGraphicsFactory.h>
namespace toolhub::graphics {
vstd::optional<GraphicsFactory_Impl> gra;
VENGINE_UNITY_EXTERN IGraphicsFactory const* Graphics_GetFactory() {
	gra.New();
	return gra;
}
}// namespace toolhub::graphics