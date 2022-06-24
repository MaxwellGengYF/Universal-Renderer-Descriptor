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
	types.emplace_back(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR);
	DescriptorSetManager descManager(device);
	ComputeShader cs(
		&descManager,
		device,
		shaderCode,
		types,
		uint3(1, 1, 1));
	types.clear();
	types.emplace_back(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

	CommandPool cmdPool(device);
	FrameResource frameRes(device, &cmdPool);
	Buffer writeBuffer(
		device,
		4,
		false,
		RWState::None);
	Buffer readbackBuffer(
		device,
		4,
		false,
		RWState::Readback);
	Buffer instUpload(
		device,
		sizeof(VkAccelerationStructureInstanceKHR),
		false,
		RWState::Upload);
	Buffer vertUpload(
		device,
		3 * sizeof(Vertex),
		false,
		RWState::Upload);
	Buffer vertBuffer(
		device,
		3 * sizeof(Vertex),
		false,
		RWState::None);
	Buffer triUpload(
		device,
		sizeof(Triangle),
		false,
		RWState::Upload);
	Buffer triBuffer(
		device,
		sizeof(Triangle),
		false,
		RWState::None);
	Mesh mesh(device);
	Vertex verts[3]{
		{float3(0.5f, 1.0f, 1.0f)},
		{float3(0.0f, 0.0f, 1.0f)},
		{float3(1.0f, 0.0f, 1.0f)}};
	Triangle t{{0, 1, 2}};
	vertUpload.CopyArrayFrom(
		verts,
		3,
		0);
	triUpload.CopyValueFrom(
		t,
		0);
	Accel accel(device);
	ResStateTracker stateTracker(device);
	vstd::vector<BuildInfo> buildInfos;
	vstd::vector<TopBuildInfo> topBuildInfos;
	vstd::vector<vstd::move_only_func<void()>> disposeFuncs;
	vstd::move_only_func<void(vstd::move_only_func<void()> &&)> addDisposeEvent = [&](auto&& v) { disposeFuncs.push_back(std::move(v)); };
	vstd::optional<Buffer> scratchBuffer;
	vstd::optional<Buffer> scratchBuffer1;
	vstd::vector<VkBufferCopy> copyRanges;
	if (auto cmdBuffer = frameRes.AllocateCmdBuffer()) {
		stateTracker.MarkBufferWrite(
			&vertBuffer,
			BufferWriteState::Copy);
		stateTracker.MarkBufferWrite(
			&triBuffer,
			BufferWriteState::Copy);
		stateTracker.Execute(cmdBuffer);
		cmdBuffer->CopyBuffer(
			&vertUpload,
			0,
			&vertBuffer,
			0,
			vertUpload.ByteSize());
		cmdBuffer->CopyBuffer(
			&triUpload,
			0,
			&triBuffer,
			0,
			triUpload.ByteSize());
		buildInfos.push_back(
			mesh.Preprocess(
				stateTracker,
				&vertBuffer,
				sizeof(Vertex),
				0,
				vertBuffer.ByteSize(),
				&triBuffer,
				0,
				triBuffer.ByteSize(),
				false, false, true, false,
				addDisposeEvent));
		size_t scratchSize = 0;
		for (auto&& i : buildInfos) {
			scratchSize += i.scratchSize;
		}
		scratchBuffer.New(
			device,
			scratchSize,
			false,
			RWState::None,
			256);
		auto&& v = buildInfos[0];
		v.scratchOffset = 0;
		v.buffer = scratchBuffer;
		mesh.Build(
			cmdBuffer->CmdBuffer(),
			buildInfos[0],
			triBuffer.ByteSize());
		buildInfos.clear();

		topBuildInfos.push_back(
			accel.Preprocess(
				cmdBuffer,
				stateTracker,
				1,
				false,
				false,
				true,
				false,
				1,
				addDisposeEvent));
		scratchSize = 0;
		for (auto&& i : topBuildInfos) {
			scratchSize += i.scratchSize;
		}
		scratchBuffer1.New(
			device,
			scratchSize,
			false,
			RWState::None,
			256);
		size_t instUploadOffset = 0;
		auto&& topBuild = topBuildInfos[0];
		topBuild.buffer = scratchBuffer1;
		topBuild.scratchOffset = 0;
		stateTracker.Execute(cmdBuffer);
		accel.SetInstance(
			0,
			&mesh,
			float4x4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1),
			&instUpload,
			instUploadOffset,
			copyRanges);

		accel.Build(stateTracker, cmdBuffer, topBuild, instUpload.GetResource(), copyRanges, 1);
		stateTracker.MarkBufferRead(
			accel.AccelBuffer(),
			BufferReadState::UseAccel);
		stateTracker.MarkBufferWrite(
			&writeBuffer,
			BufferWriteState::Compute);
		stateTracker.Execute(cmdBuffer);
		vstd::vector<BindResource> res;
		res.emplace_back(&writeBuffer, true);
		res.emplace_back(&accel);
		cmdBuffer->Dispatch(
			&cs,
			&descManager,
			res,
			uint3(1, 1, 1));
		stateTracker.MarkBufferRead(
			&writeBuffer,
			BufferReadState::ComputeOrCopy);
		stateTracker.Execute(cmdBuffer);
		cmdBuffer->CopyBuffer(
			&writeBuffer,
			0,
			&readbackBuffer,
			0,
			4);
	}
	float v = 0;
	frameRes.Execute(nullptr);
	frameRes.Wait();
	readbackBuffer.CopyValueTo(v, 0);
	std::cout << v << '\n';
	for (auto&& i : disposeFuncs) {
		i();
	}
	descManager.EndFrame();
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