#include <runtime/descriptorset_manager.h>
#include <render_graph/res_state_tracker.h>
namespace toolhub::vk {
DescriptorSetManager::DescriptorSetManager(Device const* device)
	: pool(device), Resource(device) {
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
}
namespace detail {
static thread_local DescriptorPool* gDescriptorPool;
}
DescriptorSetManager::~DescriptorSetManager() {
	pool.Destroy(samplerSet);
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
	computeWriteDescriptorSets.clear();
	computeWriteRes.clear();
	computeWriteDescriptorSets.resize(descriptors.size());
	computeWriteRes.resize(descriptors.size());
	for (auto i : vstd::range(descriptors.size())) {
		auto&& desc = descriptors[i];
#ifdef DEBUG
		switch (descTypes[i]) {
			case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
			case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
			case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
				if (!desc.res.IsTypeOf<Texture const*>()) {
					VEngine_Log("illegal binding");
					VENGINE_EXIT;
				}
				break;
			case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
			case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
				if (!desc.res.IsTypeOf<Buffer const*>()) {
					VEngine_Log("illegal binding");
					VENGINE_EXIT;
				}
				break;
			default:
				VEngine_Log("unsupported binding type");
				VENGINE_EXIT;
				break;
		}
#endif
		auto& descSet = computeWriteDescriptorSets[i];
		descSet = desc.GetDescriptor();
		computeWriteRes[i] = descSet.visit_or(VkWriteDescriptorSet(), [&](auto&& v) {
			using PtrType = std::add_pointer_t<std::remove_cvref_t<decltype(v)>>;
			return vks::initializers::writeDescriptorSet(result, descTypes[i], i, const_cast<PtrType>(&v));
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