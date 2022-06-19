#include <render_graph/res_state_tracker.h>
#include <runtime/command_buffer.h>
#include <components/buffer.h>
#include <components/texture.h>
namespace toolhub::vk {
namespace detail {
static constexpr VkAccessFlagBits GENERIC_READ_ACCESS = VkAccessFlagBits(VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT);
static constexpr VkPipelineStageFlagBits GENERIC_READ_STAGE = VkPipelineStageFlagBits(
	VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

static VkAccessFlagBits GetReadAccessFlag(BufferReadState type) {
	switch (type) {
		case BufferReadState::ComputeOrCopy:
			return GENERIC_READ_ACCESS;
		case BufferReadState::IndirectArgs:
			return VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
		case BufferReadState::Accel:
			return VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
		default:
			return VK_ACCESS_MEMORY_READ_BIT;
	}
}
static VkAccessFlagBits GetWriteAccessFlag(BufferWriteState type) {
	switch (type) {
		case BufferWriteState::Accel:
			return VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
		case BufferWriteState::Compute:
			return VK_ACCESS_SHADER_WRITE_BIT;
		case BufferWriteState::Copy:
			return VK_ACCESS_TRANSFER_WRITE_BIT;
		default:
			return VK_ACCESS_MEMORY_WRITE_BIT;
	}
}
static VkAccessFlagBits GetWriteAccessFlag(TextureWriteState type) {
	switch (type) {
		case TextureWriteState::Compute:
			return VK_ACCESS_SHADER_WRITE_BIT;
		case TextureWriteState::Copy:
			return VK_ACCESS_TRANSFER_WRITE_BIT;
		default:
			return VK_ACCESS_MEMORY_WRITE_BIT;
	}
}
static VkPipelineStageFlagBits GetStageFlag(ResourceUsage usage) {
	switch (usage) {
		case ResourceUsage::ComputeOrCopy:
			return GENERIC_READ_STAGE;
		case ResourceUsage::AccelBuild:
			return VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
		default:
			return VK_PIPELINE_STAGE_NONE;
	}
}
static VkImageLayout GetTextureLayout(VkAccessFlagBits accessFlag) {
	switch (accessFlag) {
		case GENERIC_READ_ACCESS:
			return VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
		case VK_ACCESS_TRANSFER_WRITE_BIT:
			return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		case VK_ACCESS_NONE:
			return VK_IMAGE_LAYOUT_PREINITIALIZED;
		default:
			return VK_IMAGE_LAYOUT_GENERAL;
	}
}
static VkAccessFlagBits GetTextureAccessFlag(VkImageLayout layout) {
	switch (layout) {
		case VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL:
			return GENERIC_READ_ACCESS;
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			return VK_ACCESS_TRANSFER_WRITE_BIT;
		case VK_IMAGE_LAYOUT_PREINITIALIZED:
			return VK_ACCESS_NONE;
		default:
			return VK_ACCESS_SHADER_WRITE_BIT;
	}
}
static VkPipelineStageFlagBits GetTextureUsageStage(VkImageLayout layout) {
	switch (layout) {
		case VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL:
			return GENERIC_READ_STAGE;
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			return VK_PIPELINE_STAGE_TRANSFER_BIT;
		case VK_IMAGE_LAYOUT_PREINITIALIZED:
			return VK_PIPELINE_STAGE_NONE;
		default:
			return VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
	}
}
}// namespace detail
void ResStateTracker::Reset() {
	collectedBarriers.Clear();
	readMap.Clear();
	writeMap.Clear();
	texWriteMap.Clear();
}
void ResStateTracker::MarkRange(
	BufferView const& bufferView,
	vstd::small_vector<Range>& vec, VkAccessFlagBits dstFlag, VkPipelineStageFlagBits dstStage) {
	Range selfRange(bufferView.offset, bufferView.size, dstFlag, dstStage);
	for (size_t i = 0; i < vec.size();) {
		auto& value = vec[i];
		if (!value.collide(selfRange)) {
			++i;
			continue;
		}
		VkBufferMemoryBarrier v{};
		v.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		v.srcAccessMask = value.accessFlag;
		v.dstAccessMask = dstFlag;
		v.srcQueueFamilyIndex = device->computeFamily;
		v.dstQueueFamilyIndex = device->computeFamily;
		v.buffer = bufferView.buffer->GetResource();
		v.offset = value.min;
		v.size = value.max - value.min;
		auto ite = collectedBarriers.Emplace(StagePair{value.stage, dstStage});
		ite.Value().bufferBarriers.emplace_back(v);

		if (i != (vec.size() - 1)) {
			value = vec.erase_last();
		} else {
			vec.erase_last();
			break;
		}
	}
}
void ResStateTracker::MarkTexture(
	Texture const* tex,
	uint mipLevel,
	VkAccessFlagBits dstAccess) {
	auto dstLayout = detail::GetTextureLayout(dstAccess);
	auto srcLayout = tex->TransformLayout(dstLayout, mipLevel);
	if (srcLayout == dstLayout) return;
	auto srcStage = detail::GetTextureUsageStage(srcLayout);
	auto dstStage = detail::GetTextureUsageStage(dstLayout);
	if(srcStage == 0){
		srcStage = dstStage;
	}
	auto srcAccess = detail::GetTextureAccessFlag(srcLayout);
	VkImageMemoryBarrier v{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
	v.srcAccessMask = srcAccess;
	v.dstAccessMask = dstAccess;
	v.srcQueueFamilyIndex = device->computeFamily;
	v.dstQueueFamilyIndex = device->computeFamily;
	v.oldLayout = srcLayout;
	v.newLayout = dstLayout;
	v.image = tex->GetResource();
	auto&& subRange = v.subresourceRange;
	subRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subRange.baseMipLevel = mipLevel;
	subRange.levelCount = 1;
	subRange.baseArrayLayer = 0;
	subRange.layerCount = 1;
	auto ite = collectedBarriers.Emplace(StagePair{srcStage, dstStage});
	ite.Value().imageBarriers.emplace_back(v);
}
void ResStateTracker::MarkBufferWrite(BufferView const& bufferView, BufferWriteState type, ResourceUsage usage) {
	auto ite = readMap.TryEmplace(bufferView.buffer);
	auto dstState = detail::GetWriteAccessFlag(type);
	auto dstStage = detail::GetStageFlag(usage);
	if (!ite.second) {
		MarkRange(
			bufferView,
			ite.first.Value(),
			dstState,
			dstStage);
	}
	ite = writeMap.TryEmplace(bufferView.buffer);
	auto& vec = ite.first.Value();
	if (!ite.second) {
		MarkRange(
			bufferView,
			vec,
			dstState,
			dstStage);
	}
	vec.emplace_back(bufferView.offset, bufferView.size, dstState, dstStage);
}
void ResStateTracker::MarkBufferRead(BufferView const& bufferView, BufferReadState type, ResourceUsage usage) {
	auto dstState = detail::GetReadAccessFlag(type);
	auto dstStage = detail::GetStageFlag(usage);
	auto ite = writeMap.TryEmplace(bufferView.buffer);
	if (!ite.second) {
		MarkRange(
			bufferView,
			ite.first.Value(),
			dstState,
			dstStage);
	}
	if (ite.first.Value().empty()) {
		writeMap.Remove(ite.first);
	}
	auto readIte = readMap.Emplace(bufferView.buffer);
	readIte.Value().emplace_back(bufferView.offset, bufferView.size, dstState, dstStage);
}
void ResStateTracker::MarkTextureWrite(Texture const* tex, uint targetMip, TextureWriteState type) {
	MarkTexture(
		tex, targetMip,
		detail::GetWriteAccessFlag(type));
	texWriteMap.Emplace(std::pair<Texture const*, uint>{tex, targetMip});
}
void ResStateTracker::MarkTextureRead(TexView const& tex) {
	auto dstState = detail::GENERIC_READ_ACCESS;
	auto dstLayout = detail::GetTextureLayout(dstState);
	auto dstStage = detail::GetTextureUsageStage(dstLayout);
	for (auto i : vstd::range(tex.mipOffset, tex.mipOffset + tex.mipCount)) {
		MarkTexture(
			tex.tex,
			i,
			dstState);
		texWriteMap.Remove(std::pair<Texture const*, uint>(tex.tex, i));
	}
}
void ResStateTracker::Execute(CommandBuffer* cb) {
	for (auto&& i : collectedBarriers) {
		auto& bufferBarriers = i.second.bufferBarriers;
		auto& imageBarriers = i.second.imageBarriers;
		auto srcStage = i.first.first;
		auto dstStage = i.first.second;
		vkCmdPipelineBarrier(
			cb->CmdBuffer(),
			i.first.first,
			i.first.second,
			0,
			0,
			nullptr,
			bufferBarriers.size(),
			bufferBarriers.data(),
			imageBarriers.size(),
			imageBarriers.data());
	}
	collectedBarriers.Clear();
}
ResStateTracker::ResStateTracker(Device const* device)
	: Resource(device) {}
ResStateTracker::~ResStateTracker() {}

}// namespace toolhub::vk