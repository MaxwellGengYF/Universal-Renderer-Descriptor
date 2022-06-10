#pragma once
#include <types/bind_desriptor.h>
#include <components/resource.h>
namespace toolhub::vk {
class DescriptorPool;
class ShaderCode;
class ComputeShader : public Resource {
	VkDescriptorSetLayout descriptorSetLayout;
	VkPipelineLayout pipelineLayout;
	VkPipeline pipeline;
	vstd::vector<VkDescriptorType> propertiesTypes;

public:
	ComputeShader(
		Device const* device,
		ShaderCode const& code,
		vstd::span<VkDescriptorType> properties);
	~ComputeShader();
	void BindShader(
		DescriptorPool& pool,
		vstd::span<BindDescriptor> descriptors);
};
}// namespace toolhub::vk