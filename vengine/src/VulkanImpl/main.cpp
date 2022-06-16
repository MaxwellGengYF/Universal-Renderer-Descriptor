#include <components/device.h>
#include <ve_shaderc/shaderc.hpp>
#include <Utility/BinaryReader.h>
#include <components/compute_shader.h>
#include <components/shader_code.h>
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
static void ComputeShaderTest(Device const* device, shaderc::SpvCompilationResult const& compResult) {
	uint const* ptr = compResult.begin();
	uint const* endPtr = compResult.end();
	size_t size = reinterpret_cast<size_t>(endPtr) - reinterpret_cast<size_t>(ptr);
	std::cout << "compile susccess, spirv data size: " << size << '\n';
	ShaderCode shaderCode(device, {reinterpret_cast<vbyte const*>(ptr), size}, {});
	vstd::small_vector<VkDescriptorType> types;
	types.emplace_back(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	ComputeShader cs(
		device,
		shaderCode,
		types,
		uint3(256, 1, 1));
}
int main() {
	auto instance = InitVkInstance();
	auto device = Device::CreateDevice(instance, nullptr, {}, validationLayers, 0);

	shaderc::Compiler comp;
	vstd::string shaderCode;
	{
		BinaryReader reader("test.compute");
		shaderCode = reader.ReadToString();
	}
	shaderc::CompileOptions options;
	options.SetOptimizationLevel(shaderc_optimization_level_performance);
	options.SetSourceLanguage(shaderc_source_language_glsl);
	options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
	options.SetTargetSpirv(shaderc_spirv_version_1_3);

	auto compileResult = comp.CompileGlslToSpv(
		shaderCode.data(),
		shaderCode.size(),
		shaderc_compute_shader,
		"test.compute",
		"main",
		options);
	auto errMsg = compileResult.GetErrorMessage();
	if (errMsg.size() > 0) {
		std::cout << errMsg << '\n';
		return 1;
	}
    ComputeShaderTest(device, compileResult);
	delete device;
	vkDestroyInstance(instance, Device::Allocator());
	std::cout << "finished\n";
	return 0;
}