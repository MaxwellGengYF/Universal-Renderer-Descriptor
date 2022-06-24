#pragma once
#include <vulkan_impl/gpu_collection/buffer.h>
#include "build_info.h"
#include <Common/functional.h>
namespace toolhub::vk {
class ResStateTracker;
class Buffer;
class Mesh : public GPUCollection {
	VkAccelerationStructureKHR accel = nullptr;
	vstd::unique_ptr<Buffer> accelBuffer;
	size_t lastVertSize = 0;
	size_t lastTriSize = 0;
	VkAccelerationStructureBuildGeometryInfoKHR GetBuildInfo(
		bool allowUpdate, bool allowCompact, bool fastTrace,
		bool isUpdate,
		VkAccelerationStructureGeometryKHR& geoData);
	VkAccelerationStructureGeometryKHR GetGeoInfo(
		Buffer const* vertexBuffer,
		size_t vertexStride,
		size_t vertexBufferOffset,
		size_t vertexBufferSize,
		Buffer const* triangleBuffer,
		size_t triangleBufferOffset);
	size_t TryInit(
		VkAccelerationStructureBuildGeometryInfoKHR& geometryData,
		bool allowUpdate, bool allowCompact, bool fastTrace,
		size_t triangleBufferSize);

public:
	VkAccelerationStructureKHR Accel() const { return accel; }
	Tag GetTag() const override {
		return Tag::Mesh;
	}
	Mesh(Device const* device);
	BuildInfo Preprocess(
		ResStateTracker& stateTracker,
		Buffer const* vertexBuffer,
		size_t vertexStride,
		size_t vertexBufferOffset,
		size_t vertexBufferSize,
		Buffer const* triangleBuffer,
		size_t triangleBufferOffset,
		size_t triangleBufferSize,
		bool allowUpdate, bool allowCompact, bool fastTrace, bool isUpdate,
		vstd::move_only_func<void(vstd::move_only_func<void()>&&)> const& addDisposeEvent);
	void Build(
		VkCommandBuffer cb,
		BuildInfo& buildBuffer,
		size_t triangleBufferSize);
	~Mesh();
};
}// namespace toolhub::vk