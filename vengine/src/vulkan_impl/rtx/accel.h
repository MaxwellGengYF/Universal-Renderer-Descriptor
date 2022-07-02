#pragma once
#include <gpu_collection/gpu_collection.h>
#include "build_info.h"
#include <Common/functional.h>
namespace toolhub::vk {
class Mesh;
class ResStateTracker;
class Buffer;
class CommandBuffer;
class FrameResource;
class Accel : public GPUCollection {
	vstd::unique_ptr<Buffer> instanceBuffer;//VkAccelerationStructureInstanceKHR
	vstd::unique_ptr<Buffer> accelBuffer;
	VkAccelerationStructureKHR accel = nullptr;
	size_t lastInstCount = 0;

public:
	static constexpr auto ACCEL_INST_SIZE = sizeof(VkAccelerationStructureInstanceKHR);

	VkAccelerationStructureKHR AccelNativeHandle() const { return accel; }
	Buffer* AccelBuffer() const { return accelBuffer.get(); }
	Accel(Device const* device);
	~Accel();
	VkBufferCopy SetInstance(
		size_t index,
		Mesh const* mesh,
		float4x4 const& mat,
		Buffer const* uploadBuffer,
		size_t& instByteOffset);
	TopBuildInfo Preprocess(
		CommandBuffer* cb,
		ResStateTracker& stateTracker,
		size_t buildSize,
		bool allowUpdate, bool allowCompact, bool fastTrace,
		bool isUpdate,
		size_t instanceUpdateCount,
		FrameResource* frameRes);

	void Build(
		ResStateTracker& stateTracker,
		CommandBuffer* cb,
		TopBuildInfo& buildBuffer,
		VkBuffer uploadBuffer,
		vstd::span<VkBufferCopy> instCopyCmd,
		size_t buildSize);
	Tag GetTag() const override {
		return Tag::Accel;
	}
};
}// namespace toolhub::vk