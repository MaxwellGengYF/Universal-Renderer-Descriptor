#include <components/shader_code.h>
#include <Utility/MD5.h>
namespace toolhub::vk {
ShaderCode::ShaderCode(
	Device const* device,
	vstd::vector<vbyte>&& spirvCode,
	vstd::span<vbyte const> psoCache)
	: Resource(device),
	  spirvCode(std::move(spirvCode)) {
	VkPipelineCacheCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	createInfo.flags = VK_PIPELINE_CACHE_CREATE_EXTERNALLY_SYNCHRONIZED_BIT;
	PipelineCachePrefixHeader const* ptr = reinterpret_cast<PipelineCachePrefixHeader const*>(psoCache.data());
	if (*ptr == device->psoHeader) {
		createInfo.initialDataSize = psoCache.size() - sizeof(PipelineCachePrefixHeader);
		createInfo.pInitialData = psoCache.data() + sizeof(PipelineCachePrefixHeader);
		cacheAvailable = true;
	} else {
		cacheAvailable = false;
		createInfo.initialDataSize = 0;
	}
	ThrowIfFailed(vkCreatePipelineCache(device->device, &createInfo, &device->allocator, &pipelineCache));
}
vstd::vector<vbyte> ShaderCode::GetPSOData() const {
    vstd::vector<vbyte> data;
    size_t dataSize;
    vkGetPipelineCacheData(device->device, pipelineCache, &dataSize, nullptr);
    data.resize(dataSize);
    vkGetPipelineCacheData(device->device, pipelineCache, &dataSize, data.data());
    data.resize(dataSize);
    return data;
}
ShaderCode::~ShaderCode() {
	vkDestroyPipelineCache(device->device, pipelineCache, &device->allocator);
}
}// namespace toolhub::vk