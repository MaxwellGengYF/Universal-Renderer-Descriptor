#include "accel.h"
#include "mesh.h"
#include <vulkan_impl/gpu_collection/buffer.h>
#include <vulkan_impl/runtime/res_state_tracker.h>
#include <vulkan_impl/runtime/command_buffer.h>
#include <vulkan_impl/runtime/frame_resource.h>
namespace toolhub::vk {
Accel::Accel(Device const* device)
	: GPUCollection(device) {}
Accel::~Accel() {
	if (accel) {
		device->vkDestroyAccelerationStructureKHR(device->device, accel, Device::Allocator());
	}
}
VkBufferCopy Accel::SetInstance(
	size_t index,
	Mesh const* mesh,
	float4x4 const& mat,
	Buffer const* uploadBuffer,
	size_t& instByteOffset) {
	VkAccelerationStructureInstanceKHR inst;
	memcpy(&inst.transform, &mat, sizeof(inst.transform));
	inst.mask = 0xFF;
	inst.instanceShaderBindingTableRecordOffset = 0;
	inst.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
	inst.accelerationStructureReference = GetAccelAddress(device, mesh->Accel());
	uploadBuffer->CopyValueFrom(
		inst,
		instByteOffset);

	auto cpy = VkBufferCopy{instByteOffset,
							index * ACCEL_INST_SIZE,
							ACCEL_INST_SIZE};

	instByteOffset += ACCEL_INST_SIZE;
	return cpy;
}
TopBuildInfo Accel::Preprocess(
	CommandBuffer* cb,
	ResStateTracker& stateTracker,
	size_t buildSize,
	bool allowUpdate, bool allowCompact, bool fastTrace,
	bool isUpdate,
	size_t instanceUpdateCount,
	FrameResource* frameRes) {
	isUpdate = isUpdate && (lastInstCount == buildSize);
	lastInstCount = buildSize;
	if (instanceBuffer) {
		size_t instanceCount = instanceBuffer->ByteSize() / ACCEL_INST_SIZE;
		if (instanceCount < buildSize) {
			do {
				instanceCount = instanceCount * 1.5 + 8;
			} while (instanceCount < buildSize);
			auto newBuffer = new Buffer(
				device,
				instanceCount * ACCEL_INST_SIZE,
				false,
				RWState::None);
			stateTracker.MarkBufferWrite(
				newBuffer,
				BufferWriteState::Copy);
			stateTracker.MarkBufferRead(
				instanceBuffer.get(),
				BufferReadState::ComputeOrCopy);
			stateTracker.Execute(cb);
			cb->CopyBuffer(
				instanceBuffer.get(),
				0,
				newBuffer,
				0,
				instanceBuffer->ByteSize());
			frameRes->AddDisposeEvent([v = std::move(instanceBuffer)] {});
			instanceBuffer = vstd::create_unique(newBuffer);
		}
	} else {
		instanceBuffer = vstd::create_unique(new Buffer(
			device,
			buildSize * ACCEL_INST_SIZE,
			false,
			RWState::None));
	}
	TopBuildInfo info{};
	info.instUploadBufferSize = instanceUpdateCount * ACCEL_INST_SIZE;
	GetAccelBuildInfo(info.buildInfo, true, allowUpdate, allowCompact, fastTrace, isUpdate);
	info.geoInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	info.geoInfo.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
	auto&& inst = info.geoInfo.geometry.instances;
	inst.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
	inst.arrayOfPointers = false;
	inst.data.deviceAddress = instanceBuffer->GetAddress(0);
	VkAccelerationStructureBuildSizesInfoKHR buildSizeInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
	uint instCount = buildSize;
	info.buildInfo.geometryCount = 1;
	info.buildInfo.pGeometries = &info.geoInfo;
	device->vkGetAccelerationStructureBuildSizesKHR(
		device->device,
		VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&info.buildInfo, &instCount,
		&buildSizeInfo);
	auto accelSize = buildSizeInfo.accelerationStructureSize;
	if (accelBuffer) {
		size_t newSize = accelBuffer->ByteSize();
		if (newSize < accelSize) {
			do {
				newSize = newSize * 1.5 + 8;
			} while (newSize < accelSize);
			newSize = CalcAlign(newSize, 256);
			frameRes->AddDisposeEvent([a = std::move(accelBuffer)] {});
			accelBuffer = vstd::create_unique(new Buffer(
				device,
				newSize,
				false,
				RWState::Accel,
				256));
		}
	} else {
		accelBuffer = vstd::create_unique(new Buffer(
			device,
			CalcAlign(accelSize, 256),
			false,
			RWState::Accel,
			256));
	}
	stateTracker.MarkBufferWrite(
		accelBuffer.get(),
		BufferWriteState::Accel);
	if (instanceUpdateCount > 0) {
		stateTracker.MarkBufferWrite(
			instanceBuffer.get(),
			BufferWriteState::Copy);
	}
	info.scratchSize = isUpdate ? buildSizeInfo.updateScratchSize : buildSizeInfo.buildScratchSize;
	if (!isUpdate) {
		if (accel) {
			frameRes->AddDisposeEvent([v = std::move(instanceBuffer), accel = accel, device = device] {
				device->vkDestroyAccelerationStructureKHR(device->device, accel, Device::Allocator());
			});
		}
		VkAccelerationStructureCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
		createInfo.buffer = accelBuffer->GetResource();
		createInfo.offset = 0;
		createInfo.size = buildSize;
		createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		ThrowIfFailed(device->vkCreateAccelerationStructureKHR(
			device->device,
			&createInfo,
			Device::Allocator(),
			&accel));
	}
	info.isUpdate = isUpdate;
	return info;
}
void Accel::Build(
	ResStateTracker& stateTracker,
	CommandBuffer* cb,
	TopBuildInfo& buildBuffer,
	VkBuffer uploadBuffer,
	vstd::span<VkBufferCopy> instCopyCmd,
	size_t buildSize) {
	if (!instCopyCmd.empty()) {
		vkCmdCopyBuffer(
			cb->CmdBuffer(),
			uploadBuffer,
			instanceBuffer->GetResource(),
			instCopyCmd.size(),
			instCopyCmd.data());
		stateTracker.MarkBufferRead(
			instanceBuffer.get(),
			BufferReadState::BuildAccel);
		stateTracker.Execute(cb);
	}
	auto&& buildInfo = buildBuffer.buildInfo;
	buildInfo.geometryCount = 1;
	buildInfo.pGeometries = &buildBuffer.geoInfo;
	if (buildBuffer.isUpdate) {
		buildInfo.srcAccelerationStructure = accel;
	}
	buildInfo.scratchData.deviceAddress = buildBuffer.buffer->GetAddress(buildBuffer.scratchOffset);
	buildInfo.dstAccelerationStructure = accel;
	VkAccelerationStructureBuildRangeInfoKHR buildRange;
	buildRange.primitiveCount = buildSize;
	buildRange.primitiveOffset = 0;
	buildRange.firstVertex = 0;
	buildRange.transformOffset = 0;
	auto ptr = &buildRange;
	device->vkCmdBuildAccelerationStructuresKHR(
		cb->CmdBuffer(),
		1,
		&buildInfo,
		&ptr);
}
}// namespace toolhub::vk