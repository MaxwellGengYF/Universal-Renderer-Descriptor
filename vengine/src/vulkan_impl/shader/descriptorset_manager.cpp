#include <vulkan_impl/shader/descriptorset_manager.h>
#include <vulkan_impl/runtime/res_state_tracker.h>
#include <vulkan_impl/gpu_collection/buffer.h>
#include <vulkan_impl/gpu_collection/texture.h>
#include <vulkan_impl/rtx/accel.h>
#include <vulkan_impl/runtime/res_state_tracker.h>
namespace toolhub::vk {

namespace detail {
static thread_local DescriptorPool* gDescriptorPool;
static vstd::vector<DescriptorSetManager*> managers;
static vstd::spin_mutex managerMtx;
}// namespace detail
DescriptorSetManager::DescriptorSetManager(Device const* device)
	: Resource(device),
	  stackAlloc(4096, &device->mallocVisitor) {
	std::lock_guard lck(detail::managerMtx);
	glbIndex = detail::managers.size();
	detail::managers.emplace_back(this);
}
DescriptorSetManager::~DescriptorSetManager() {
	detail::gDescriptorPool = device->pool.get();
	descSets.Clear();
	std::lock_guard lck(detail::managerMtx);
	auto last = detail::managers.erase_last();
	if (glbIndex != detail::managers.size()) {
		detail::managers[glbIndex] = last;
		last->glbIndex = glbIndex;
	}
}
DescriptorSetManager::DescriptorSets::~DescriptorSets() {
	if (!sets.empty())
		detail::gDescriptorPool->Destroy(sets);
}

void DescriptorSetManager::DestroyPipelineLayout(VkDescriptorSetLayout layout) {
	std::lock_guard lck(detail::managerMtx);
	for (auto&& i : detail::managers) {
		i->removeList.Push(layout);
	}
}
VkDescriptorSet DescriptorSetManager::Allocate(
	ResStateTracker& stateTracker,
	VkDescriptorSetLayout layout,
	vstd::span<VkDescriptorType const> descTypes,
	vstd::span<BindResource const> descriptors) {
	while (auto v = removeList.Pop()) {
		detail::gDescriptorPool = device->pool.get();
		descSets.Remove(*v);
	}
	stackAlloc.Clear();
	assert(descTypes.size() == descriptors.size());
	decltype(descSets)::Index ite;
	ite = descSets.Find(layout);
	if (!ite) {
		ite = descSets.Emplace(layout);
	}
	VkDescriptorSet result;
	{
		std::lock_guard lck(ite.Value().mtx);
		auto&& vec = ite.Value().sets;
		if (vec.empty())
			result = device->pool->Allocate(layout);
		else
			result = vec.erase_last();
	}
	allocatedSets.emplace_back(ite, result);
	computeWriteRes.clear();
	computeWriteRes.resize(descriptors.size());
	memset(computeWriteRes.data(), 0, computeWriteRes.byte_size());
	for (auto i : vstd::range(descriptors.size())) {
		auto&& desc = descriptors[i];
		auto&& writeDst = computeWriteRes[i];
		desc.res.multi_visit(
			[&](Texture const* tex) {
				if (desc.writable) {
					stateTracker.MarkTextureWrite(
						tex, desc.offset, TextureWriteState::Compute);
				} else {
					stateTracker.MarkTextureRead(TexView(tex, desc.offset, desc.size));
				}
				auto&& descType = descTypes[i];
#ifdef DEBUG
				if (descType != VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
					&& descType != VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE
					&& descType != VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) {
					VEngine_Log("illegal binding");
					VENGINE_EXIT;
				}
#endif
				auto bf = stackAlloc.Allocate(sizeof(VkDescriptorImageInfo));
				auto ptr = reinterpret_cast<VkDescriptorImageInfo*>(bf.handle + bf.offset);
				*ptr = tex->GetDescriptor(
					desc.offset,
					desc.size);
				writeDst = vks::initializers::writeDescriptorSet(
					result, descType, i, ptr);
			},
			[&](Buffer const* buffer) {
				auto bfView = BufferView(buffer, desc.offset, desc.size);
				if (desc.writable) {
					stateTracker.MarkBufferWrite(
						bfView,
						BufferWriteState::Compute);
				} else {
					stateTracker.MarkBufferRead(
						bfView,
						BufferReadState::ComputeOrCopy);
				}

				auto&& descType = descTypes[i];
#ifdef DEBUG
				if (descType != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
					&& descType != VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) {
					VEngine_Log("illegal binding");
					VENGINE_EXIT;
				}
#endif
				auto bf = stackAlloc.Allocate(sizeof(VkDescriptorBufferInfo));
				auto ptr = reinterpret_cast<VkDescriptorBufferInfo*>(bf.handle + bf.offset);
				*ptr = buffer->GetDescriptor(
					desc.offset,
					desc.size);
				writeDst = vks::initializers::writeDescriptorSet(
					result, descType, i, ptr);
			},
			[&](Accel const* accel) {
				stateTracker.MarkBufferRead(
					accel->AccelBuffer(),
					BufferReadState::UseAccel);
#ifdef DEBUG
				auto&& descType = descTypes[i];
				if (descType != VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR
					&& descType != VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV) {
					VEngine_Log("illegal binding");
					VENGINE_EXIT;
				}
#endif
				auto bf = stackAlloc.Allocate(sizeof(VkWriteDescriptorSetAccelerationStructureKHR));
				auto ptr = reinterpret_cast<VkWriteDescriptorSetAccelerationStructureKHR*>(bf.handle + bf.offset);
				bf = stackAlloc.Allocate(sizeof(VkAccelerationStructureKHR));
				auto accelPtr = reinterpret_cast<VkAccelerationStructureKHR*>(bf.handle + bf.offset);
				*accelPtr = accel->AccelNativeHandle();

				ptr->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
				ptr->pNext = nullptr;
				ptr->accelerationStructureCount = 1;
				ptr->pAccelerationStructures = accelPtr;

				writeDst.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				writeDst.pNext = ptr;
				writeDst.dstSet = result;
				writeDst.dstBinding = i;
				writeDst.descriptorCount = 1;
				writeDst.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

				//writeDst
			});
	}
	vkUpdateDescriptorSets(device->device, computeWriteRes.size(), computeWriteRes.data(), 0, nullptr);
	return result;
}
void DescriptorSetManager::EndFrame() {
	for (auto&& i : allocatedSets) {
		i.first.Value().sets.emplace_back(i.second);
	}
	allocatedSets.clear();
}
}// namespace toolhub::vk