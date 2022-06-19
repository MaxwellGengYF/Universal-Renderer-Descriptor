#include <components/device.h>
#include <ve_shaderc/shaderc.hpp>
#include <Utility/BinaryReader.h>
#include <components/compute_shader.h>
#include <components/shader_code.h>
#include <components/buffer.h>
#include <components/descriptor_pool.h>
#include <runtime/descriptorset_manager.h>
#include <runtime/command_pool.h>
#include <runtime/frame_resource.h>
#include <render_graph/res_state_tracker.h>
#include <components/texture.h>

using namespace toolhub::vk;
static auto validationLayers = {"VK_LAYER_KHRONOS_validation"};
static VkInstance InitVkInstance() {
	VkInstance instance;
#ifdef DEBUG
	auto Check = [&] {
		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		vstd::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
		for (const char* layerName : validationLayers) {
			bool layerFound = false;

			for (const auto& layerProperties : availableLayers) {
				if (strcmp(layerName, layerProperties.layerName) == 0) {
					layerFound = true;
					break;
				}
			}

			if (!layerFound) {
				return false;
			}
		}

		return true;
	};
	if (!Check()) {
		VEngine_Log("validation layers requested, but not available!");
		VENGINE_EXIT;
	}
#endif
	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = nullptr;
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = nullptr;
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VulkanApiVersion;

	VkInstanceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;
	vstd::vector<char const*> requiredExts;
	{
/*		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions;
		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		requiredExts = vstd::span<const char*>{glfwExtensions, glfwExtensionCount};*/
#ifdef DEBUG
		requiredExts.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif
	}
	createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExts.size());
	createInfo.ppEnabledExtensionNames = requiredExts.data();

	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
#ifdef DEBUG
	createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
	createInfo.ppEnabledLayerNames = validationLayers.begin();
	auto populateDebugMessengerCreateInfo = [](VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
		createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		createInfo.pfnUserCallback =
			[](VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
			   VkDebugUtilsMessageTypeFlagsEXT messageType,
			   const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
			   void* pUserData) {
				if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
					// Message is important enough to show
					std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
				}

				return VK_FALSE;
			};
	};
	populateDebugMessengerCreateInfo(debugCreateInfo);
	createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
#else
	createInfo.enabledLayerCount = 0;
	createInfo.pNext = nullptr;
#endif
	// Create Instance
	ThrowIfFailed(vkCreateInstance(&createInfo, Device::Allocator(), &instance));
	return instance;
}
static void ComputeShaderTest(Device const* device, shaderc::SpvCompilationResult const& compResult,
							  shaderc::SpvCompilationResult const& writeResult) {
	uint const* ptr = compResult.begin();
	uint const* endPtr = compResult.end();
	size_t size = reinterpret_cast<size_t>(endPtr) - reinterpret_cast<size_t>(ptr);
	ShaderCode shaderCode(device, {reinterpret_cast<vbyte const*>(ptr), size}, {});
	ptr = writeResult.begin();
	endPtr = writeResult.end();
	size = reinterpret_cast<size_t>(endPtr) - reinterpret_cast<size_t>(ptr);
	ShaderCode writeShaderCode(device, {reinterpret_cast<vbyte const*>(ptr), size}, {});
	vstd::small_vector<VkDescriptorType> types;
	types.emplace_back(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	types.emplace_back(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	DescriptorSetManager descManager(device);
	ComputeShader cs(
		&descManager,
		device,
		shaderCode,
		types,
		uint3(256, 1, 1));
	types.clear();
	types.emplace_back(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

	ComputeShader writeCS(
		&descManager,
		device,
		writeShaderCode,
		types,
		uint3(256, 1, 1));
	CommandPool cmdPool(device);
	FrameResource frameRes(device, &cmdPool);
	Buffer uploadBuffer(
		device,
		1024 * sizeof(float),
		false,
		RWState::Upload);
	ResStateTracker stateTracker(device);
	vstd::vector<float> value(1024);
	uploadBuffer.CopyFrom({reinterpret_cast<vbyte const*>(value.data()), value.byte_size()}, 0);
	Buffer readbackBuffer(
		device,
		1024 * sizeof(float),
		false,
		RWState::Readback);

	Buffer buffer(
		device,
		1024 * sizeof(float),
		false,
		RWState::None);
	Texture tex(
		device,
		uint3(1024, 1024, 1),
		VK_FORMAT_R32_SFLOAT,
		VK_IMAGE_TYPE_2D,
		1);

	if (auto cmdBuffer = frameRes.AllocateCmdBuffer()) {
		vstd::small_vector<BindResource> binder;
		binder.emplace_back(TexView(&tex), true);
		cmdBuffer->PreprocessDispatch(
			stateTracker,
			binder);
		stateTracker.Execute(cmdBuffer);
		cmdBuffer->Dispatch(
			&writeCS,
			&descManager,
			binder,
			uint3(1024, 1, 1));

		binder.clear();
		binder.emplace_back(BufferView(&buffer), true);
		binder.emplace_back(TexView(&tex), false);
		cmdBuffer->PreprocessDispatch(
			stateTracker,
			binder);
		stateTracker.Execute(cmdBuffer);
		cmdBuffer->Dispatch(
			&cs,
			&descManager,
			binder,
			uint3(1024, 1, 1));
		cmdBuffer->PreprocessCopyBuffer(
			stateTracker,
			&buffer,
			0,
			&readbackBuffer,
			0,
			value.byte_size());

		cmdBuffer->CopyBuffer(
			&buffer,
			0,
			&readbackBuffer,
			0,
			value.byte_size());
	}
	frameRes.Execute(nullptr);
	frameRes.Wait();
	descManager.EndFrame();
	readbackBuffer.CopyTo({reinterpret_cast<vbyte*>(value.data()), value.byte_size()}, 0);
	for (auto&& i : value) {
		std::cout << i << ' ';
	}
}
int main() {
	auto instance = InitVkInstance();
	auto device = Device::CreateDevice(instance, nullptr, {}, validationLayers, 0);

	shaderc::Compiler comp;
	vstd::string shaderCode;
	vstd::string writeTexCode;
	{
		BinaryReader reader("test.compute");
		shaderCode = reader.ReadToString();
	}
	{
		BinaryReader reader("write_tex.compute");
		writeTexCode = reader.ReadToString();
	}
	shaderc::CompileOptions options;
	options.SetOptimizationLevel(shaderc_optimization_level_performance);
	options.SetSourceLanguage(shaderc_source_language_hlsl);
	options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
	options.SetTargetSpirv(shaderc_spirv_version_1_3);

	auto compileResult = comp.CompileGlslToSpv(
		shaderCode.data(),
		shaderCode.size(),
		shaderc_compute_shader,
		"test.compute",
		"main",
		options);
	auto writeTexCompileResult = comp.CompileGlslToSpv(
		writeTexCode.data(),
		writeTexCode.size(),
		shaderc_compute_shader,
		"write_tex.compute",
		"main",
		options);

	auto errMsg = compileResult.GetErrorMessage();
	if (errMsg.size() > 0) {
		std::cout << errMsg << '\n';
		return 1;
	}
	errMsg = writeTexCompileResult.GetErrorMessage();
	if (errMsg.size() > 0) {
		std::cout << errMsg << '\n';
		return 1;
	}
	ComputeShaderTest(device, compileResult, writeTexCompileResult);
	delete device;
	vkDestroyInstance(instance, Device::Allocator());
	std::cout << "finished\n";
	return 0;
}