#pragma once
#include <Common/Common.h>
#include <Eigen/Eigen>
#include <glm/Include.h>
using namespace glm;
namespace toolhub::graphics {
class Voxel {
	Eigen::Matrix4f localToWorld;
};
class PackedVoxel {
	vec3 right;
	vec3 up;
	vec3 forward;
	vec3 position;
};
}// namespace toolhub::graphics