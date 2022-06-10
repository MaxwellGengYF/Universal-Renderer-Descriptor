#include <compute_shader.h>
#include <vulkan_initializer.hpp>
#include <components/descriptor_pool.h>
#include <utility/shader_utility.h>
#include <components/shader_code.h>
namespace toolhub::vk {
ComputeShader::ComputeShader(
	Device const* device,
	ShaderCode const& code,
	vstd::span<VkDescriptorType> properties)
	: propertiesTypes(properties),
	  Resource(device) {

	vstd::vector<VkDescriptorSetLayoutBinding, VEngine_AllocType::VEngine, 16> setLayoutBindings;
	setLayoutBindings.push_back_func(properties.size(), [&](size_t i) {
		return vks::initializers::descriptorSetLayoutBinding(properties[i], VK_SHADER_STAGE_COMPUTE_BIT, i);
	});

	VkDescriptorSetLayoutCreateInfo descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
	ThrowIfFailed(vkCreateDescriptorSetLayout(device->device, &descriptorLayout, &device->allocator, &descriptorSetLayout));

	VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo =
		vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);

	ThrowIfFailed(vkCreatePipelineLayout(device->device, &pPipelineLayoutCreateInfo, &device->allocator, &pipelineLayout));
	VkComputePipelineCreateInfo computePipelineCreateInfo =
		vks::initializers::computePipelineCreateInfo(pipelineLayout, 0);
	VkPipelineShaderStageCreateInfo shaderStage = {};
	shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	shaderStage.module = ShaderUtility::LoadShader(code.SpirvCode(), device->device);
	shaderStage.pName = "main";
	ThrowIfFailed(vkCreateComputePipelines(device->device, code.PipelineCache(), 1, &computePipelineCreateInfo, &device->allocator, &pipeline));
}

void ComputeShader::BindShader(
	DescriptorPool& pool,
	vstd::span<BindDescriptor> descriptors) {
	auto descriptorSet = pool.Allocate(descriptorSetLayout);
	vstd::vector<VkWriteDescriptorSet, VEngine_AllocType::VEngine, 16> computeWriteDescriptorSets;
	computeWriteDescriptorSets.push_back_func(descriptors.size(), [&](size_t i) {
		return descriptors[i].visit_or(VkWriteDescriptorSet(), [&](auto&& v) {
			return vks::initializers::writeDescriptorSet(descriptorSet, propertiesTypes[i], i, &v);
		});
	});
	vkUpdateDescriptorSets(device->device, computeWriteDescriptorSets.size(), computeWriteDescriptorSets.data(), 0, nullptr);
}
ComputeShader::~ComputeShader() {
	vkDestroyPipeline(device->device, pipeline, &device->allocator);
}
}// namespace toolhub::vk