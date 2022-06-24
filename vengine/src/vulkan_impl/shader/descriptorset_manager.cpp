#include <vulkan_impl/shader/descriptorset_manager.h>
#include <vulkan_impl/runtime/res_state_tracker.h>
#include <vulkan_impl/gpu_collection/buffer.h>
#include <vulkan_impl/gpu_collection/texture.h>
#include <vulkan_impl/rtx/accel.h>
namespace toolhub::vk {
void DescriptorSetManager::InitBindless() {
	VkDescriptorSetLayoutBinding setLayoutBinding{};
	setLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	setLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	setLayoutBinding.binding = 0;
	setLayoutBinding.descriptorCount = DescriptorPool::MAX_BINDLESS_SIZE;
	VkDescriptorSetLayoutCreateInfo descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo({&setLayoutBinding, 1});

	VkDescriptorSetLayoutBindingFlagsCreateInfoEXT setLayoutBindingFlags{};
	setLayoutBindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT;
	setLayoutBindingFlags.bindingCount = 1;
	VkDescriptorBindingFlagsEXT descriptorBindingFlags = VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT;
	setLayoutBindingFlags.pBindingFlags = &descriptorBindingFlags;
	descriptorLayout.pNext = &setLayoutBindingFlags;
	ThrowIfFailed(vkCreateDescriptorSetLayout(device->device, &descriptorLayout, Device::Allocator(), &bindlessTexSetLayout));
	bindlessTexSet = pool.Allocate(bindlessTexSetLayout);
	setLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	ThrowIfFailed(vkCreateDescriptorSetLayout(device->device, &descriptorLayout, Device::Allocator(), &bindlessBufferSetLayout));
	bindlessBufferSet = pool.Allocate(bindlessBufferSetLayout);
}
DescriptorSetManager::DescriptorSetManager(Device const* device)
	: pool(device), Resource(device),
	  stackAlloc(4096, &mallocVisitor) {
	// create sampler desc-set layout
	VkDescriptorSetLayoutBinding binding =
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 0, DescriptorPool::MAX_SAMP);
	VkDescriptorSetLayoutCreateInfo descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo({&binding, 1});
	ThrowIfFailed(vkCreateDescriptorSetLayout(device->device, &descriptorLayout, Device::Allocator(), &samplerSetLayout));
	samplerSet = pool.Allocate(samplerSetLayout);
	VkFilter filters[4] = {
		VK_FILTER_NEAREST,
		VK_FILTER_LINEAR,
		VK_FILTER_LINEAR,
		VK_FILTER_LINEAR};
	VkSamplerMipmapMode mipFilter[4] = {
		VK_SAMPLER_MIPMAP_MODE_NEAREST,
		VK_SAMPLER_MIPMAP_MODE_NEAREST,
		VK_SAMPLER_MIPMAP_MODE_LINEAR,
		VK_SAMPLER_MIPMAP_MODE_LINEAR};
	VkSamplerAddressMode addressMode[4] = {
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		VK_SAMPLER_ADDRESS_MODE_REPEAT,
		VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER};

	size_t idx = 0;
	VkDescriptorImageInfo imageInfos[DescriptorPool::MAX_SAMP];
	for (auto x : vstd::range(4))
		for (auto y : vstd::range(4)) {
			auto d = vstd::create_disposer([&] { ++idx; });
			auto&& samp = samplers[idx];
			VkSamplerCreateInfo createInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
			createInfo.magFilter = filters[y];
			createInfo.minFilter = filters[y];
			createInfo.mipmapMode = mipFilter[y];
			createInfo.addressModeU = addressMode[x];
			createInfo.addressModeV = addressMode[x];
			createInfo.addressModeW = addressMode[x];
			createInfo.mipLodBias = 0;
			createInfo.anisotropyEnable = y == 3;
			createInfo.maxAnisotropy = DescriptorPool::MAX_SAMP;
			createInfo.minLod = 0;
			createInfo.maxLod = 255;
			vkCreateSampler(
				device->device,
				&createInfo,
				Device::Allocator(),
				&samp);
			imageInfos[idx].sampler = samp;
		}
	VkWriteDescriptorSet writeDesc{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
	writeDesc.dstSet = samplerSet;
	writeDesc.dstBinding = 0;
	writeDesc.dstArrayElement = 0;
	writeDesc.descriptorCount = DescriptorPool::MAX_SAMP;
	writeDesc.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
	writeDesc.pImageInfo = imageInfos;
	vkUpdateDescriptorSets(device->device, 1, &writeDesc, 0, nullptr);
	InitBindless();
}
namespace detail {
static thread_local DescriptorPool* gDescriptorPool;
}
DescriptorSetManager::~DescriptorSetManager() {
	pool.Destroy(samplerSet);
	pool.Destroy(bindlessTexSet);
	pool.Destroy(bindlessBufferSet);
	vkDestroyDescriptorSetLayout(
		device->device,
		bindlessTexSetLayout,
		Device::Allocator());
	vkDestroyDescriptorSetLayout(
		device->device,
		bindlessBufferSetLayout,
		Device::Allocator());
	detail::gDescriptorPool = &pool;
	descSets.Clear();
	vkDestroyDescriptorSetLayout(
		device->device,
		samplerSetLayout,
		Device::Allocator());
	for (auto&& i : samplers) {
		vkDestroySampler(device->device, i, Device::Allocator());
	}
}
DescriptorSetManager::DescriptorSets::~DescriptorSets() {
	if (!sets.empty())
		detail::gDescriptorPool->Destroy(sets);
}

void DescriptorSetManager::DestroyPipelineLayout(VkDescriptorSetLayout layout) {
	detail::gDescriptorPool = &pool;
	std::lock_guard lck(mtx);
	descSets.Remove(layout);
}
VkDescriptorSet DescriptorSetManager::Allocate(
	VkDescriptorSetLayout layout,
	vstd::span<VkDescriptorType const> descTypes,
	vstd::span<BindResource const> descriptors) {
	stackAlloc.Clear();
	assert(descTypes.size() == descriptors.size());
	decltype(descSets)::Index ite;
	{
		std::shared_lock lck(mtx);
		ite = descSets.Find(layout);
		if (!ite) {
			lck.unlock();
			std::lock_guard lck(mtx);
			ite = descSets.Emplace(layout);
		}
	}
	VkDescriptorSet result;
	{
		std::lock_guard lck(ite.Value().mtx);
		auto&& vec = ite.Value().sets;
		if (vec.empty())
			result = pool.Allocate(layout);
		else
			result = vec.erase_last();
	}
	allocatedSets.emplace_back(ite, result);
	computeWriteRes.clear();
	computeWriteRes.resize(descriptors.size());
	for (auto i : vstd::range(descriptors.size())) {
		auto&& desc = descriptors[i];
		auto&& writeDst = computeWriteRes[i];
		desc.res.multi_visit(
			[&](Texture const* tex) {
				auto&& descType = descTypes[i];
#ifdef DEBUG
				if (descType != VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
					|| descType != VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE
					|| descType != VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) {
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
				auto&& descType = descTypes[i];
#ifdef DEBUG
				if (descType != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
					|| descType != VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) {
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
#ifdef DEBUG
				auto&& descType = descTypes[i];
				if (descType != VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR
					|| descType != VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV) {
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
		/*
		auto& descSet = computeWriteDescriptorSets[i];
		descSet = desc.GetDescriptor();
		computeWriteRes[i] = descSet.visit_or(VkWriteDescriptorSet(), [&](auto&& v) {
			using PtrType = std::add_pointer_t<std::remove_cvref_t<decltype(v)>>;
			return vks::initializers::writeDescriptorSet(result, descTypes[i], i, const_cast<PtrType>(&v));
		});*/
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