#include <Common/Common.h>
#include <glm/Include.h>
using namespace glm;
namespace toolhub {
struct TRS {
	vec3 position;
	vec3 right;
	vec3 up;
	vec3 forward;
	vec3 bboxCenter;
	vec3 bboxExtent;
};
struct CamArgs {
	vec3 cameraRight;
	vec3 cameraUp;
	vec3 cameraForward;
	vec3 cameraPosition;
	float fov;
	float aspect;
	float resolution;
	float nearPlane;
	float farPlane;
};
struct ProjectBBox {
	vec3 center;
	vec3 extent;
};
}// namespace toolhub