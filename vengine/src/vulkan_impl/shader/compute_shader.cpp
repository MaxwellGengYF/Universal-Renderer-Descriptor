#include <vulkan_impl/shader/compute_shader.h>
#include <Common/small_vector.h>
#include <vulkan_impl/vulkan_initializer.hpp>
#include <vulkan_impl/shader/descriptor_pool.h>
#include <vulkan_impl/utility/shader_utility.h>
#include <vulkan_impl/shader/shader_code.h>
#include <vulkan_impl/shader/descriptorset_manager.h>
namespace toolhub::vk {
ComputeShader::ComputeShader(
	Device const* device,
	ShaderCode const& code,
	vstd::span<VkDescriptorType> properties,
	uint3 threadGroupSize)
	: propertiesTypes(properties),
	  threadGroupSize(threadGroupSize),
	  Resource(device) {

	vstd::small_vector<VkDescriptorSetLayoutBinding> setLayoutBindings;
	setLayoutBindings.push_back_func(properties.size(), [&](size_t i) {
		return vks::initializers::descriptorSetLayoutBinding(properties[i], VK_SHADER_STAGE_COMPUTE_BIT, i);
	});

	VkDescriptorSetLayoutCreateInfo descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
	ThrowIfFailed(vkCreateDescriptorSetLayout(device->device, &descriptorLayout, Device::Allocator(), &descriptorSetLayout));
	//TODO: add bindless array descset layout
	// bindless descset should be in Device class
	VkDescriptorSetLayout layouts[] = {
		descriptorSetLayout,
		device->bindlessBufferSetLayout,
		device->bindlessTex2DSetLayout,
		device->bindlessTex3DSetLayout,
		device->samplerSetLayout};
	VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo =
		vks::initializers::pipelineLayoutCreateInfo(layouts, vstd::array_count(layouts));

	ThrowIfFailed(vkCreatePipelineLayout(device->device, &pPipelineLayoutCreateInfo, Device::Allocator(), &pipelineLayout));
	VkComputePipelineCreateInfo computePipelineCreateInfo =
		vks::initializers::computePipelineCreateInfo(pipelineLayout, 0);
	VkPipelineShaderStageCreateInfo shaderStage = {};
	shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	shaderStage.module = ShaderUtility::LoadShader(code.SpirvCode(), device->device);
	shaderStage.pName = "main";
	computePipelineCreateInfo.stage = shaderStage;
	auto d = vstd::create_disposer([&] {
		vkDestroyShaderModule(device->device, shaderStage.module, Device::Allocator());
	});
	ThrowIfFailed(vkCreateComputePipelines(device->device, code.PipelineCache(), 1, &computePipelineCreateInfo, Device::Allocator(), &pipeline));
}

ComputeShader::~ComputeShader() {
	DescriptorSetManager::DestroyPipelineLayout(descriptorSetLayout);
	vkDestroyPipeline(device->device, pipeline, Device::Allocator());
	vkDestroyPipelineLayout(device->device, pipelineLayout, Device::Allocator());
	vkDestroyDescriptorSetLayout(device->device, descriptorSetLayout, Device::Allocator());
}
}// namespace toolhub::vk