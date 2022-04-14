#pragma once
#include <Common/Common.h>
#include <Eigen/Eigen>
#include "Voxel.h"
#include "VoxelBVHNode.h"
namespace toolhub::graphics {
class VoxelBVHUtil {
public:
	using VoxelPool = vstd::Pool<VoxelBVHNode, VEngine_AllocType::VEngine, true>;
	using NodePool = vstd::Pool<VoxelBVHNode, VEngine_AllocType::VEngine, true>;
	static void Pack(
		VoxelBVHNode const& root,
		vstd::vector<PackedVoxel>& voxels,
		vstd::vector<PackedVoxelBVHNode>& nodes);
	static void Dispose(
		VoxelBVHNode const& root,
		VoxelPool& voxelPool,
		NodePool& nodePool);
};
}// namespace toolhub::graphics