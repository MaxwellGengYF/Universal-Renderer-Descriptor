#pragma once
#define VK_USE_PLATFORM_WIN32_KHR
#include <Common/Common.h>
#include <vulkan/vulkan.h>
class GLFWwindow;
namespace toolhub::vulkan {
class HelloTriangleApplication {
	struct QueueFamilyIndices {
		vstd::optional<uint32_t> graphicsFamily;
		vstd::optional<uint32_t> presentFamily;
		bool isComplete() {
			return graphicsFamily.has_value() && presentFamily.has_value();
		}
	};
	struct SwapChainSupportDetails {
		VkSurfaceCapabilitiesKHR capabilities;
		vstd::vector<VkSurfaceFormatKHR> formats;
		vstd::vector<VkPresentModeKHR> presentModes;
	};

	static constexpr uint32_t WIDTH = 800;
	static constexpr uint32_t HEIGHT = 600;
	vstd::vector<const char*> validationLayers;
	GLFWwindow* window;
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VkInstance instance;
	VkDevice device;
	VkSurfaceKHR surface;
	VkQueue graphicsQueue;
	VkQueue presentQueue;
	VkSwapchainKHR swapChain;
	QueueFamilyIndices indices;
	VkShaderModule vertShaderModule;
	VkShaderModule fragShaderModule;
	VkPipelineLayout pipelineLayout;
	VkPipeline graphicsPipeline;
	VkRenderPass renderPass;
	vstd::vector<const char*> requiredExts;
	vstd::vector<const char*> deviceExtensions;
	vstd::vector<VkImage> swapChainImages;
	vstd::vector<VkImageView> swapChainImageViews;
	vstd::vector<VkFramebuffer> swapChainFramebuffers;
	VkFormat swapChainImageFormat;
	VkExtent2D swapChainExtent;
	VkCommandPool commandPool;
	VkCommandBuffer commandBuffer;

	VkSemaphore imageAvailableSemaphore;
	VkSemaphore renderFinishedSemaphore;
	VkFence inFlightFence;

#ifdef DEBUG
	VkDebugUtilsMessengerEXT debugMessenger;
	void CheckValidationLayerSupport();
	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData);
	void setupDebugMessenger();
	void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
#endif
	void createSurface();
	bool checkDeviceExtensionSupport(VkPhysicalDevice device);
	VkResult CreateDebugUtilsMessengerEXT(
		VkInstance instance,
		const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
		const VkAllocationCallbacks* pAllocator,
		VkDebugUtilsMessengerEXT* pDebugMessenger);
	void DestroyDebugUtilsMessengerEXT(
		VkInstance instance,
		VkDebugUtilsMessengerEXT debugMessenger,
		const VkAllocationCallbacks* pAllocator);
	void CreateInstance();
	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
	bool isDeviceSuitable(VkPhysicalDevice device);
	void pickPhysicalDevice();
	void createLogicalDevice();
	SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
	VkSurfaceFormatKHR chooseSwapSurfaceFormat(const vstd::vector<VkSurfaceFormatKHR>& availableFormats);
	VkPresentModeKHR chooseSwapPresentMode(const vstd::vector<VkPresentModeKHR>& availablePresentModes);
	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
	void createSwapChain();
	void createImageViews();
	static vstd::vector<vbyte> readFile(const vstd::string& filename);
	VkShaderModule createShaderModule(const vstd::vector<vbyte>& code);
	void createGraphicsPipeline();
	void createRenderPass();
	void createFrameBuffers();
	void createCommandPool();
	void createCommandBuffer();
	void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
	void drawFrame();
	void createSyncObjects();
public:
	void Run();
	HelloTriangleApplication();
	~HelloTriangleApplication();
};
}// namespace toolhub::vulkan