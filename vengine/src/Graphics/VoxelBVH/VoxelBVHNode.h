#pragma once
#include <Common/Common.h>
#include <Eigen/Eigen>
namespace toolhub::graphics {
class Voxel;
class PackedVoxel;
class PackedVoxelBVHNode;
class VoxelBVHNode {
public:
	vstd::variant<
		std::pair<VoxelBVHNode*, VoxelBVHNode*>,
		vstd::vector<Voxel*>>
		vars;
	Eigen::Vector3f center;
	Eigen::Vector3f size;
};
class PackedVoxelBVHNode {
	uint start;
	//zero for subnode
	//non-zero for primitive
	uint size;
};
}// namespace toolhub::graphics