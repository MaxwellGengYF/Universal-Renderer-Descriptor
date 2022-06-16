#pragma once
#include <types/bind_desriptor.h>
#include <components/resource.h>
#include <Common/small_vector.h>
namespace toolhub::vk {
class DescriptorSetManager;
class ShaderCode;
class ComputeShader : public Resource {
	VkDescriptorSetLayout descriptorSetLayout;
	VkPipelineLayout pipelineLayout;
	VkPipeline pipeline;
	vstd::small_vector<VkDescriptorType> propertiesTypes;
	uint3 threadGroupSize;

public:
	ComputeShader(
		Device const* device,
		ShaderCode const& code,
		vstd::span<VkDescriptorType> properties,
		uint3 threadGroupSize);
	~ComputeShader();
	//TODO
	void Dispatch(
		vstd::span<BindDescriptor> descriptors,
		DescriptorSetManager& manager,
		uint3 dispatchCount);
		
};
}// namespace toolhub::vk