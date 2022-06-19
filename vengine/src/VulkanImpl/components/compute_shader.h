#pragma once
#include <types/bind_desriptor.h>
#include <components/resource.h>
#include <Common/small_vector.h>
namespace toolhub::vk {
class DescriptorSetManager;
class ShaderCode;
class CommandBuffer;

class ComputeShader : public Resource {
	friend class CommandBuffer;
	VkDescriptorSetLayout descriptorSetLayout;
	VkPipelineLayout pipelineLayout;
	VkPipeline pipeline;
	vstd::small_vector<VkDescriptorType> propertiesTypes;
	uint3 threadGroupSize;

public:
	uint3 ThreadGroupSize() const { return threadGroupSize; }
	ComputeShader(
		DescriptorSetManager const* descManager,
		Device const* device,
		ShaderCode const& code,
		vstd::span<VkDescriptorType> properties,
		uint3 threadGroupSize);
	~ComputeShader();
};
}// namespace toolhub::vk