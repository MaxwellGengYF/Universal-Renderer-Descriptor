#include <vulkan_impl/components/device.h>
#include <Utility/BinaryReader.h>
#include <vulkan_impl/shader/compute_shader.h>
#include <vulkan_impl/shader/shader_code.h>
#include <vulkan_impl/gpu_collection/buffer.h>
#include <vulkan_impl/shader/descriptor_pool.h>
#include <vulkan_impl/shader/descriptorset_manager.h>
#include <vulkan_impl/runtime/command_pool.h>
#include <vulkan_impl/runtime/frame_resource.h>
#include <vulkan_impl/runtime/res_state_tracker.h>
#include <vulkan_impl/gpu_collection/texture.h>
#include <vulkan_impl/rtx/mesh.h>
#include <vulkan_impl/rtx/accel.h>
#include <dxc/dxc_util.h>
#include <vulkan_impl/rtx/query.h>
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
struct alignas(16) Vertex {
	float3 pos;
};
struct Triangle {
	uint v[3];
};
static void ComputeShaderTest(Device const* device, toolhub::directx::DXByteBlob const& block) {
	vbyte* ptr = block.GetBufferPtr();
	size_t size = block.GetBufferSize();
	ShaderCode shaderCode(device, {ptr, size}, {});

	vstd::small_vector<VkDescriptorType> types;
	types.emplace_back(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	DescriptorSetManager descManager(device);
	ComputeShader cs(
		&descManager,
		device,
		shaderCode,
		types,
		uint3(1, 1, 1));
	Buffer defaultBuffer(
		device,
		sizeof(float),
		false,
		RWState::None,
		0);
	Buffer outputDefault(
		device,
		sizeof(float),
		false,
		RWState::None,
		0);
	descManager.AddBindlessUpdateCmd(
		15,
		&defaultBuffer);
	descManager.UpdateBindless();

	CommandPool cmdPool(device);
	FrameResource frameRes(device, &cmdPool);

	ResStateTracker stateTracker(device);
	vstd::vector<vstd::move_only_func<void()>> disposeFuncs;
	vstd::move_only_func<void(vstd::move_only_func<void()> &&)> addDisposeEvent = [&](auto&& v) { disposeFuncs.push_back(std::move(v)); };
	BufferView readback;
	if (auto cmdBuffer = frameRes.AllocateCmdBuffer()) {
		auto upload = frameRes.AllocateUpload(4);
		readback = frameRes.AllocateReadback(4);
		float inputFloat = 365;
		upload.buffer->CopyValueFrom(inputFloat, upload.offset);
		stateTracker.MarkBufferWrite(
			&defaultBuffer,
			BufferWriteState::Copy);
		stateTracker.Execute(cmdBuffer);
		cmdBuffer->CopyBuffer(
			upload.buffer,
			upload.offset,
			&defaultBuffer,
			0,
			4);
		stateTracker.MarkBufferRead(
			&defaultBuffer,
			BufferReadState::ComputeOrCopy);
		stateTracker.MarkBufferWrite(
			&outputDefault,
			BufferWriteState::Compute);
		stateTracker.Execute(cmdBuffer);
		vstd::vector<BindResource> binds;
		binds.emplace_back(&outputDefault, true);
		cmdBuffer->Dispatch(
			&cs,
			&descManager,
			binds,
			uint3(1, 1, 1));
		stateTracker.MarkBufferRead(
			&outputDefault,
			BufferReadState::ComputeOrCopy);
		stateTracker.Execute(cmdBuffer);
		cmdBuffer->CopyBuffer(
			&outputDefault,
			0,
			readback.buffer,
			readback.offset,
			4);
	}
	frameRes.Execute(nullptr);
	frameRes.Wait();
	for (auto&& i : disposeFuncs) {
		i();
	}
	descManager.EndFrame();
	float readbackValue = 0;
	readback.buffer->CopyValueTo(readbackValue, readback.offset);
	std::cout << "result[0] value: " << readbackValue << '\n';
}
int main() {
	auto instance = InitVkInstance();
	auto device = Device::CreateDevice(instance, nullptr, validationLayers, 0);
	vstd::string shaderCode;
	{
		BinaryReader reader("test.compute");
		shaderCode = reader.ReadToString();
	}
	toolhub::directx::DXShaderCompiler compiler;
	auto compResult = compiler.CompileCompute(
		shaderCode,
		true);
	auto errMsg = compResult.try_get<vstd::string>();
	if (errMsg) {
		std::cout << *errMsg << '\n';
		return 1;
	}
	/*
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
	}*/
	ComputeShaderTest(device, *compResult.get<0>());
	delete device;
	vkDestroyInstance(instance, Device::Allocator());
	std::cout << "finished\n";
	return 0;
}