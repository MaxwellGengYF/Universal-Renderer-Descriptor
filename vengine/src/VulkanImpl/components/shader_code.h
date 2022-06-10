#pragma once
#include <components/resource.h>
namespace toolhub::vk {
class ShaderCode : public Resource {
	vstd::vector<vbyte> spirvCode;
	VkPipelineCache pipelineCache;
	bool cacheAvailable;

public:
	bool CacheAvailable() const { return cacheAvailable; }
	vstd::span<vbyte const> SpirvCode() const { return spirvCode; }
	VkPipelineCache PipelineCache() const { return pipelineCache; }
	vstd::vector<vbyte> GetPSOData() const;
	ShaderCode(
		Device const* device,
		vstd::vector<vbyte>&& spirvCode,
		vstd::span<vbyte const> psoCache);
	~ShaderCode();
};
}// namespace toolhub::vk