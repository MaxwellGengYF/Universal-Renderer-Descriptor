#pragma vengine_dll vengine_graphics
#include "VoxelBVHUtil.h"
namespace toolhub::graphics {
void VoxelBVHUtil::Pack(
	VoxelBVHNode const& root,
	vstd::vector<PackedVoxel>& voxels,
	vstd::vector<PackedVoxelBVHNode>& nodes) {}
void VoxelBVHUtil::Dispose(
	VoxelBVHNode const& root,
	VoxelPool& voxelPool,
	NodePool& nodePool) {}
}// namespace toolhub::graphics