#pragma once
#include <vulkan_impl/types/bind_desriptor.h>
#include <vulkan_impl/components/resource.h>
#include <Common/small_vector.h>
namespace toolhub::vk {
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
		Device const* device,
		ShaderCode const& code,
		vstd::span<VkDescriptorType> properties,
		uint3 threadGroupSize);
	~ComputeShader();
};
}// namespace toolhub::vk