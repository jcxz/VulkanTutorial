#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <functional>
#include <cstdlib>
#include <vector>
#include <set>
#include <optional>
#include <string>
#include <algorithm>
#include <fstream>


static std::vector<char> readFile(const std::string& filename)
{
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open())
	{
		throw std::runtime_error("Failed to open file!");
	}

	const size_t fileSize = (const size_t) file.tellg();
	std::vector<char> buffer(fileSize);

	file.seekg(0);
	file.read(buffer.data(), fileSize);

	file.close();

	return buffer;
}


static const int WIDTH = 800;
static const int HEIGHT = 600;
// defines how many frames should be processed concurrently
static const int MAX_FRAMES_IN_FLIGHT = 2;

static const char* const g_validationLayers[] =
{
	"VK_LAYER_LUNARG_standard_validation",
};

static const uint32_t g_validationLayersCount = sizeof(g_validationLayers) / sizeof(g_validationLayers[0]);

#ifdef NDEBUG
static const bool g_enableValidationLayers = false;
#else
static const bool g_enableValidationLayers = true;
#endif

static const char* const g_deviceExtensions[] =
{
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

static const uint32_t g_deviceExtensionsCount = sizeof(g_deviceExtensions) / sizeof(g_deviceExtensions[0]);


static VkResult CreateDebugUtilsMessengerEXT(
	VkInstance instance,
	const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
	const VkAllocationCallbacks* pAllocator,
	VkDebugUtilsMessengerEXT* pDebugMessenger)
{
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr)
	{
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	}
	else
	{
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

static void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator)
{
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr)
	{
		func(instance, debugMessenger, pAllocator);
	}
}

static void printSupportedVulkanExtensions()
{
	VkResult err = VK_SUCCESS;

	uint32_t extensionCount = 0;
	// prvy parameter je layerName
	err = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
	if (err != VK_SUCCESS)
	{
		std::cerr << "Failed to get the number of Vulkan extensions." << std::endl;
		return;
	}

	std::vector<VkExtensionProperties> extensions(extensionCount);
	err = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());
	if (err != VK_SUCCESS)
	{
		std::cerr << "Failed to get the Vulkan extensions." << std::endl;
		return;
	}

	std::cout << "Vulkan Instance Extensions:" << std::endl;
	std::cout << "\tName | version" << std::endl;
	for (const VkExtensionProperties& extension : extensions)
	{
		std::cout << "\t" << extension.extensionName << " | " << extension.specVersion << std::endl;
	}
}

static bool checkValidationLayerSupport(const char* const * const pLayers, const int nLayers)
{
	VkResult res = VK_SUCCESS;

	uint32_t layerCount = 0;
	res = vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
	if (res != VK_SUCCESS)
	{
		std::cerr << "Failed to get the count of Vulkan instace layers." << std::endl;
		return false;
	}

	std::vector<VkLayerProperties> layers(layerCount);
	res = vkEnumerateInstanceLayerProperties(&layerCount, layers.data());
	if (res != VK_SUCCESS)
	{
		std::cerr << "Failed to get Vulkan instace layers." << std::endl;
		return false;
	}

	// print Vulkan Instace layers
	std::cout << "Vulkan Instance Layers:" << std::endl;
	std::cout << "\tName | spec version | implementation version | description" << std::endl;
	for (const VkLayerProperties& layer : layers)
	{
		std::cout << "\t" << layer.layerName << " | " << layer.specVersion << " | " << layer.implementationVersion << " | " << layer.description << std::endl;
	}

	for (int i = 0; i < nLayers; ++i)
	{
		bool layerFound = false;

		for (const VkLayerProperties& layer : layers)
		{
			if (std::strcmp(pLayers[i], layer.layerName) == 0)
			{
				layerFound = true;
				break;
			}
		}

		if (!layerFound)
		{
			std::cerr << "Validation layer " << pLayers[i] << " not found" << std::endl;
			return false;
		}
	}

	return true;
}

static std::string queueFlagsToString(const VkQueueFlags flags)
{
	std::string s;

	if (flags & VK_QUEUE_GRAPHICS_BIT)
		s += "VK_QUEUE_GRAPHICS_BIT";

	if (flags & VK_QUEUE_COMPUTE_BIT)
	{
		if (s.empty())
			s += "VK_QUEUE_COMPUTE_BIT";
		else
			s += " | VK_QUEUE_COMPUTE_BIT";
	}

	if (flags & VK_QUEUE_TRANSFER_BIT)
	{
		if (s.empty())
			s += "VK_QUEUE_TRANSFER_BIT";
		else
			s += " | VK_QUEUE_TRANSFER_BIT";
	}

	if (flags & VK_QUEUE_SPARSE_BINDING_BIT)
	{
		if (s.empty())
			s += "VK_QUEUE_SPARSE_BINDING_BIT";
		else
			s += " | VK_QUEUE_SPARSE_BINDING_BIT";
	}

	if (flags & VK_QUEUE_PROTECTED_BIT)
	{
		if (s.empty())
			s += "VK_QUEUE_PROTECTED_BIT";
		else
			s += " | VK_QUEUE_PROTECTED_BIT";
	}

	return s;
}

static std::ostream& operator<<(std::ostream& os, const VkExtent3D& extent)
{
	return os << "VkExtent3D(" << extent.width << ", " << extent.height << ", " << extent.depth << ")";
}

struct QueueFamilyIndices
{
	std::optional<uint32_t> graphicsFamily;
	std::optional<uint32_t> presentFamily;

	bool isComplete() const
	{
		return graphicsFamily.has_value() && presentFamily.has_value();
	}
};

struct SwapChainSupportDetails
{
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};



class HelloTriangleApplication
{
public:
	void run()
	{
		initWindow();
		initVulkan();
		mainLoop();
		cleanup();
	}

private:
	void initWindow()
	{
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		//glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

		m_window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);

		glfwSetFramebufferSizeCallback(m_window, framebufferResizeCallback);
		glfwSetWindowUserPointer(m_window, this);
	}

	// ///////////////////////////////////////////////////////////////////////////////////////////////
	// VULKAN initialization
	// ///////////////////////////////////////////////////////////////////////////////////////////////

#pragma region VULKAN_init
	void initVulkan()
	{
		std::cout << "*** Creating VULKAN instance ..." << std::endl;
		createInstance();

		std::cout << "*** Setting up debug messenger ..." << std::endl;
		setupDebugMessenger();

		// surface needs to be created before picking a physical device,
		// because it can influce this process ...
		std::cout << "*** Creating surface ..." << std::endl;
		createSurface();

		std::cout << "*** Selecting physical VULKAN device ..." << std::endl;
		pickPhysicalDevice();

		std::cout << "*** Creating logical VULKAN device and a command queue ..." << std::endl;
		createLogicalDevice();

		std::cout << "*** Creating swapchain ..." << std::endl;
		createSwapChain();

		std::cout << "*** Creating image views for images in swapchain ..." << std::endl;
		createImageViews();

		std::cout << "*** Creating VULKAN render pass ..." << std::endl;
		createRenderPass();

		std::cout << "*** Creating VULKAN graphics pipeline ..." << std::endl;
		createGraphicsPipeline();

		std::cout << "*** Creating framebuffers ..." << std::endl;
		createFramebuffers();

		std::cout << "*** Creating Command Pool ..." << std::endl;
		createCommandPool();

		std::cout << "*** Creating Command Buffers ..." << std::endl;
		createCommandBuffers();

		std::cout << "*** Creating synchronization objects ..." << std::endl;
		createSyncObjects();
	}

	void createInstance()
	{
		printSupportedVulkanExtensions();

		if (g_enableValidationLayers && !checkValidationLayerSupport(g_validationLayers, g_validationLayersCount))
		{
			throw std::runtime_error("Validation layers requested, but not available!");
		}

		// tato strukturka je optional, ale moze driveru poskytnut uzitocne informacie o aplikacii
		VkApplicationInfo appInfo = {};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Hello Triangle";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "No Engine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		// must be the highest version of Vulkan that the application is designed to use
		appInfo.apiVersion = VK_API_VERSION_1_0;

		// tato struktura nie je optional a hovori driveru, ktore global extensions a validation layers bude program pouzivat
		// tuto sa nastavi globalne pre cely program, ake validation layers sa budu pouzivat
		VkInstanceCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;

		// interfacovanie s windowing system potrebujem extension a glfw uz obsahuje funkciu, ktory vrati zoznam extensions, ktore glfw potrebuje
		std::vector<const char*> extensions = getRequiredExtensions();
		createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		createInfo.ppEnabledExtensionNames = extensions.data();

		std::cout << "Required extensions:" << std::endl;
		for (const char* extension : extensions)
		{
			std::cout << '\t' << extension << std::endl;
		}

		if (g_enableValidationLayers)
		{
			std::cout << "Enabling debug layers ..." << std::endl;
			createInfo.enabledLayerCount = g_validationLayersCount;
			createInfo.ppEnabledLayerNames = g_validationLayers;
		}
		else
		{
			createInfo.enabledLayerCount = 0;
		}

		VkResult result = vkCreateInstance(&createInfo, nullptr, &m_instance);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create Vulkan Instance");
		}

		std::cout << "Vulkan instance was successfuly created." << std::endl;
	}

	std::vector<const char*> getRequiredExtensions()
	{
		uint32_t glfwExtensionCount = 0;
		const char * const * glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

		if (g_enableValidationLayers)
		{
			// tuto extension je treba, aby mi validation layer mohla posielat debug spravy do mojej aplikacie
			// VK_EXT_DEBUG_UTILS_EXTENSION_NAME je len makro pre stringovy literal "VK_EXT_debug_utils"
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}

		return extensions;
	}

	void setupDebugMessenger()
	{
		if (!g_enableValidationLayers)
			return;

		VkDebugUtilsMessengerCreateInfoEXT createInfo = { };
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		createInfo.pfnUserCallback = debugCallback;
		createInfo.pUserData = nullptr; // Optional

		VkResult res = CreateDebugUtilsMessengerEXT(m_instance, &createInfo, nullptr, &m_debugMessenger);
		if (res != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to set up debug messenger!");
			return;
		}
	}

	void createSurface()
	{
		// calls vkCreateWin32SurfaceKHR or similar according to the actual platform (e.g. vkCreateXcbSurfaceKHR, ...)
		VkResult res = glfwCreateWindowSurface(m_instance, m_window, nullptr, &m_surface);
		if (res != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create window surface!");
		}
	}

	void pickPhysicalDevice()
	{
		VkResult res = VK_SUCCESS;

		uint32_t deviceCount = 0;
		res = vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
		if (res != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to find GPUs with Vulkan support!");
		}

		std::vector<VkPhysicalDevice> devices(deviceCount);
		res = vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());
		if (res != VK_SUCCESS)
		{
			std::cerr << "Failed to get physical Vulkan devices." << std::endl;
			return;
		}

		QueueFamilyIndices queueFamilyIndices;
		for (const VkPhysicalDevice& device : devices)
		{
			if (isDeviceSuitable(device, queueFamilyIndices))
			{
				m_physicalDevice = device;
				m_graphicsQueueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
				m_presentationQueueFamilyIndex = queueFamilyIndices.presentFamily.value();
				break;
			}
		}

		if (m_physicalDevice == VK_NULL_HANDLE)
		{
			throw std::runtime_error("Failed to find a suitable GPU!");
		}
	}

	bool isDeviceSuitable(const VkPhysicalDevice device, QueueFamilyIndices& indices)
	{
		// provides basic device properties like the name, type and supported Vulkan version
		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(device, &deviceProperties);

		std::cout << "Checking device " << deviceProperties.deviceName << " ..." << std::endl;

		// querries the support for optional features like texture compression, 64 bit floats and multi viewport rendering(useful for VR)
		//VkPhysicalDeviceFeatures deviceFeatures;
		//vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

		indices = findQueueFamilies(device);

		bool extensionsSupported = checkDeviceExtensionSupport(device);

		bool swapChainAdequate = false;
		if (extensionsSupported)
		{
			// Just checking if a swap chain is available is not sufficient, because it may not actually be compatible with our window surface.
			SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
			swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
		}

		return indices.isComplete() && extensionsSupported && swapChainAdequate;
	}

	QueueFamilyIndices findQueueFamilies(const VkPhysicalDevice device)
	{
		QueueFamilyIndices indices;

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

		// print information about all available queue families
		std::cout << "\tQueue Flags | Queue count | timestamp | transfer Granularity" << std::endl;
		for (const VkQueueFamilyProperties& family : queueFamilies)
		{
			std::cout << "\t(" << queueFlagsToString(family.queueFlags) << ") | " << family.queueCount << " | " << family.timestampValidBits << " | " << family.minImageTransferGranularity << std::endl;
		}

		int i = 0;
		for (const auto& queueFamily : queueFamilies)
		{
			if (queueFamily.queueCount > 0)
			{
				if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
					indices.graphicsFamily = i;

				VkBool32 presentSupport = false;
				vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &presentSupport);

				if (presentSupport)
					indices.presentFamily = i;
			}

			if (indices.isComplete())
			{
				break;
			}

			++i;
		}

		return indices;
	}

	bool checkDeviceExtensionSupport(const VkPhysicalDevice device)
	{
		uint32_t extensionCount;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

		std::set<std::string> requiredExtensions(g_deviceExtensions, g_deviceExtensions + g_deviceExtensionsCount);

		std::cout << "Vulkan Device Extensions:" << std::endl;
		std::cout << "\tName | version" << std::endl;
		for (const auto& extension : availableExtensions)
		{
			std::cout << "\t" << extension.extensionName << " | " << extension.specVersion << std::endl;
			requiredExtensions.erase(extension.extensionName);
		}

		return requiredExtensions.empty();
	}

	SwapChainSupportDetails querySwapChainSupport(const VkPhysicalDevice device)
	{
		SwapChainSupportDetails details;
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &details.capabilities);

		uint32_t formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, nullptr);

		if (formatCount != 0)
		{
			details.formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, details.formats.data());
		}

		uint32_t presentationModeCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentationModeCount, nullptr);

		if (presentationModeCount != 0)
		{
			details.presentModes.resize(presentationModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentationModeCount, details.presentModes.data());
		}

		return details;
	}

	void createSwapChain()
	{
		SwapChainSupportDetails swapChainSupport = querySwapChainSupport(m_physicalDevice);

		VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
		VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
		VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

		uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
		if ((swapChainSupport.capabilities.maxImageCount > 0) && (imageCount > swapChainSupport.capabilities.maxImageCount))
		{
			imageCount = swapChainSupport.capabilities.maxImageCount;
		}

		std::cout
			<< "imageCount = " << imageCount
			<< ", swapChainSupport.capabilities.minImageCount = " << swapChainSupport.capabilities.minImageCount
			<< ", swapChainSupport.capabilities.maxImageCount = " << swapChainSupport.capabilities.maxImageCount
			<< std::endl;

		VkSwapchainCreateInfoKHR createInfo = { };
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = m_surface;
		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = extent;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		uint32_t queueFamilyIndices[] = { m_graphicsQueueFamilyIndex, m_presentationQueueFamilyIndex };

		//  If the graphics queue family and presentation queue family are the same, we should stick to exclusive mode,
		// because concurrent mode requires you to specify at least two distinct queue families.
		if (m_graphicsQueueFamilyIndex != m_presentationQueueFamilyIndex)
		{
			// Images can be used across multiple queue families without explicit ownership transfers.
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;
		}
		else
		{
			// An image is owned by one queue family at a time and ownership must be explicitly transfered before using it in another queue family. This option offers the best performance.
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			createInfo.queueFamilyIndexCount = 0;       // Optional
			createInfo.pQueueFamilyIndices = nullptr;   // Optional
		}

		createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
		// blendovanie s ostatnymi oknami window managera, teda ze ci ma byt okno, ktore vytvaram transparentne
		// The compositeAlpha field specifies if the alpha channel should be used for blending with other windows in the window system.
		// You'll almost always want to simply ignore the alpha channel, hence VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR.
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.presentMode = presentMode;
		// If the clipped member is set to VK_TRUE then that means that we don't care about the color of pixels that are obscured,
		// for example because another window is in front of them.
		// Unless you really need to be able to read these pixels back and get predictable results, you'll get the best performance by enabling clipping.
		createInfo.clipped = VK_TRUE;
		// toto treba vyplnit, ak znovu vytvaram swapchain (napr. pri window resize)
		createInfo.oldSwapchain = VK_NULL_HANDLE;

		const VkResult res = vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapChain);
		if (res != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create swap chain!");
		}

		vkGetSwapchainImagesKHR(m_device, m_swapChain, &imageCount, nullptr);

		m_swapChainImages.resize(imageCount);
		vkGetSwapchainImagesKHR(m_device, m_swapChain, &imageCount, m_swapChainImages.data());

		m_swapChainImageFormat = surfaceFormat.format;
		m_swapChainExtent = extent;
	}

	VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
	{
		// The best case scenario is that the surface has no preferred format, which Vulkan indicates
		// by only returning one VkSurfaceFormatKHR entry which has its format member set to VK_FORMAT_UNDEFINED.
		if (availableFormats.size() == 1 && availableFormats[0].format == VK_FORMAT_UNDEFINED)
		{
			return { VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
		}

		for (const auto& availableFormat : availableFormats)
		{
			if ((availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM) && (availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR))
			{
				return availableFormat;
			}
		}

		return availableFormats[0];
	}

	VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
	{
		VkPresentModeKHR bestMode = VK_PRESENT_MODE_FIFO_KHR; // this is the only mode that is guaranteed by Vulkan to be supported

		for (const auto& availablePresentMode : availablePresentModes)
		{
			// VK_PRESENT_MODE_MAILBOX_KHR should basically be tripple buffering
			// always prefer this one
			if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
			{
				return availablePresentMode;
			}
			// VK_PRESENT_MODE_FIFO_KHR is not properly supported by all drivers, so prefer immediate mode to this one
			else if (availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR)
			{
				bestMode = availablePresentMode;
			}
		}

		return bestMode;
	}

	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
	{
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
		{
			return capabilities.currentExtent;
		}
		else
		{
			int width, height;
			glfwGetFramebufferSize(m_window, &width, &height);

			VkExtent2D actualExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };

			actualExtent.width  = std::clamp(actualExtent.width,  capabilities.minImageExtent.width,  capabilities.maxImageExtent.width);
			actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
			return actualExtent;
		}
	}

	void createLogicalDevice()
	{
#if 0
		float queuePriority = 1.0f;

		VkDeviceQueueCreateInfo queueCreateInfos[2] = { };
		queueCreateInfos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfos[0].pNext = nullptr;
		queueCreateInfos[0].flags = 0;
		queueCreateInfos[0].queueFamilyIndex = m_graphicsQueueFamilyIndex;
		queueCreateInfos[0].queueCount = 1;
		queueCreateInfos[0].pQueuePriorities = &queuePriority;

		queueCreateInfos[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfos[1].pNext = nullptr;
		queueCreateInfos[1].flags = 0;
		queueCreateInfos[1].queueFamilyIndex = m_presentationQueueFamilyIndex;
		queueCreateInfos[1].queueCount = 1;
		queueCreateInfos[1].pQueuePriorities = &queuePriority;

		VkPhysicalDeviceFeatures deviceFeatures = { };

		VkDeviceCreateInfo createInfo = { };
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.queueCreateInfoCount = 2;
		createInfo.pQueueCreateInfos = queueCreateInfos;
		// Previous implementations of Vulkan made a distinction between instance and device specific validation layers, but this is no longer the case.
		// That means that the enabledLayerCount and ppEnabledLayerNames fields of VkDeviceCreateInfo are ignored by up-to-date implementations.
		// However, it is still a good idea to set them anyway to be compatible with older implementations.
		createInfo.enabledLayerCount = g_enableValidationLayers ? g_validationLayersCount : 0;
		createInfo.ppEnabledLayerNames = g_enableValidationLayers ? g_validationLayers : nullptr;
		createInfo.enabledExtensionCount = 0;
		createInfo.ppEnabledExtensionNames = nullptr;
		createInfo.pEnabledFeatures = &deviceFeatures;

		VkResult ret = vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device);
		if (ret != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create logical device!");
		}

		// tu sa pytam na handle prvej queue z "pola" graphicsQueue (mozem mat aj viacej skutocnych queue) pre rodinu graphics queues
		vkGetDeviceQueue(m_device, m_graphicsQueueFamilyIndex, 0, &m_graphicsQueue);

		// get the handle of the presentation queue
		vkGetDeviceQueue(m_device, m_presentationQueueFamilyIndex, 0, &m_presentationQueue);
#else
		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		std::set<uint32_t> uniqueQueueFamilies = { m_graphicsQueueFamilyIndex, m_presentationQueueFamilyIndex };

		float queuePriority = 1.0f;
		for (uint32_t queueFamily : uniqueQueueFamilies)
		{
			VkDeviceQueueCreateInfo queueCreateInfo = { };
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = queueFamily;
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;
			queueCreateInfos.push_back(queueCreateInfo);
		}

		VkPhysicalDeviceFeatures deviceFeatures = { };

		VkDeviceCreateInfo createInfo = { };
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
		createInfo.pQueueCreateInfos = queueCreateInfos.data();
		// Previous implementations of Vulkan made a distinction between instance and device specific validation layers, but this is no longer the case.
		// That means that the enabledLayerCount and ppEnabledLayerNames fields of VkDeviceCreateInfo are ignored by up-to-date implementations.
		// However, it is still a good idea to set them anyway to be compatible with older implementations.
		createInfo.enabledLayerCount = g_enableValidationLayers ? g_validationLayersCount : 0;
		createInfo.ppEnabledLayerNames = g_enableValidationLayers ? g_validationLayers : nullptr;
		createInfo.enabledExtensionCount = g_deviceExtensionsCount;
		createInfo.ppEnabledExtensionNames = g_deviceExtensions;
		createInfo.pEnabledFeatures = &deviceFeatures;

		VkResult ret = vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device);
		if (ret != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create logical device!");
		}

		// tu sa pytam na handle prvej queue z "pola" graphicsQueue (mozem mat aj viacej skutocnych queue) pre rodinu graphics queues
		vkGetDeviceQueue(m_device, m_graphicsQueueFamilyIndex, 0, &m_graphicsQueue);

		// get the handle of the presentation queue
		vkGetDeviceQueue(m_device, m_presentationQueueFamilyIndex, 0, &m_presentationQueue);
#endif
	}

	void createImageViews()
	{
		m_swapChainImageViews.resize(m_swapChainImages.size());
		const size_t n = m_swapChainImages.size();
		for (size_t i = 0; i < n; ++i)
		{
			VkImageViewCreateInfo createInfo = { };
			createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			createInfo.image = m_swapChainImages[i];
			createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			createInfo.format = m_swapChainImageFormat;

			createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
			// The subresourceRange field describes what the image's purpose is and which part of the image should be accessed.
			// Our images will be used as color targets without any mipmapping levels or multiple layers.
			//
			// If you were working on a stereographic 3D application, then you would create a swap chain with multiple layers.
			// You could then create multiple image views for each image representing the views for the left and right eyes by accessing different layers.
			createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			createInfo.subresourceRange.baseMipLevel = 0;
			createInfo.subresourceRange.levelCount = 1;
			createInfo.subresourceRange.baseArrayLayer = 0;
			createInfo.subresourceRange.layerCount = 1;

			VkResult res = vkCreateImageView(m_device, &createInfo, nullptr, &m_swapChainImageViews[i]);
			if (res != VK_SUCCESS)
			{
				throw std::runtime_error("failed to create image views!");
			}
		}
	}

	void createGraphicsPipeline()
	{
		std::vector<char> vertShaderCode = readFile("shaders/vert.spv");
		std::vector<char> fragShaderCode = readFile("shaders/frag.spv");

		std::cout << "Successfully loaded vertex shader code, size = " << vertShaderCode.size() << std::endl;
		std::cout << "Successfully loaded fragment shader code, size = " << fragShaderCode.size() << std::endl;

		VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
		VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

		VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
		vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertShaderStageInfo.module = vertShaderModule;
		vertShaderStageInfo.pName = "main";

		VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
		fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragShaderStageInfo.module = fragShaderModule;
		fragShaderStageInfo.pName = "main";

		// Allows one to specify values for shader constants.
		// You can use a single shader module where its behavior can be configured at pipeline creation by specifying different values for the constants used in it.

		//fragShaderStageInfo.pSpecializationInfo = ... ;

		VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

		// *** TATO CAST DEFINUJE VSETKY FIXED FUNCTION PARAMETRE AKO NAPRIKLAD BLENDOVANIE, DEPTH-TESTING, VERTEX INPUT, ...

		VkPipelineVertexInputStateCreateInfo vertexInputInfo = { };
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputInfo.vertexBindingDescriptionCount = 0;
		vertexInputInfo.pVertexBindingDescriptions = nullptr; // Optional
		vertexInputInfo.vertexAttributeDescriptionCount = 0;
		vertexInputInfo.pVertexAttributeDescriptions = nullptr; // Optional

		VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo = { };
		inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		// typ primitiva, ktore budem renderovat
		inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		// Normally, the vertices are loaded from the vertex buffer by index in sequential order, but with an element buffer you can specify the indices to use yourself.
		// This allows you to perform optimizations like reusing vertices. If you set the primitiveRestartEnable member to VK_TRUE, then it's possible to break up lines
		// and triangles in the _STRIP topology modes by using a special index of 0xFFFF or 0xFFFFFFFF.
		inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

		VkViewport viewport = { };
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (float)m_swapChainExtent.width;
		viewport.height = (float)m_swapChainExtent.height;
		// to iste ako glDepthRange, musi byt v rozsahu [0, 1]
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		// While viewports define the transformation from the image to the framebuffer, scissor rectangles define in which regions pixels will actually be stored.
		// Any pixels outside the scissor rectangles will be discarded by the rasterizer. They function like a filter rather than a transformation.
		VkRect2D scissor = { };
		scissor.offset = { 0, 0 };
		scissor.extent = m_swapChainExtent;

		VkPipelineViewportStateCreateInfo viewportStateInfo = { };
		viewportStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportStateInfo.viewportCount = 1;
		viewportStateInfo.pViewports = &viewport;
		viewportStateInfo.scissorCount = 1;
		viewportStateInfo.pScissors = &scissor;

		VkPipelineRasterizationStateCreateInfo rasterizerStateInfo = { };
		rasterizerStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizerStateInfo.depthClampEnable = VK_FALSE;
		// If rasterizerDiscardEnable is set to VK_TRUE, then geometry never passes through the rasterizer stage. This basically disables any output to the framebuffer.
		rasterizerStateInfo.rasterizerDiscardEnable = VK_FALSE;
		// Using any mode other than fill requires enabling a GPU feature.
		rasterizerStateInfo.polygonMode = VK_POLYGON_MODE_FILL;
		// The maximum line width that is supported depends on the hardware and any line thicker than 1.0f requires you to enable the wideLines GPU feature.
		rasterizerStateInfo.lineWidth = 1.0f;

		rasterizerStateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterizerStateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;

		rasterizerStateInfo.depthBiasEnable = VK_FALSE;
		rasterizerStateInfo.depthBiasConstantFactor = 0.0f; // Optional
		rasterizerStateInfo.depthBiasClamp = 0.0f; // Optional
		rasterizerStateInfo.depthBiasSlopeFactor = 0.0f; // Optional

		// Enabling multisampling requires enabling a GPU feature.
		VkPipelineMultisampleStateCreateInfo multisamplingStateInfo = { };
		multisamplingStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisamplingStateInfo.sampleShadingEnable = VK_FALSE;
		multisamplingStateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisamplingStateInfo.minSampleShading = 1.0f; // Optional
		multisamplingStateInfo.pSampleMask = nullptr; // Optional
		multisamplingStateInfo.alphaToCoverageEnable = VK_FALSE; // Optional
		multisamplingStateInfo.alphaToOneEnable = VK_FALSE; // Optional

		// tato strukturka je per framebuffer
		VkPipelineColorBlendAttachmentState colorBlendAttachment = { };
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.blendEnable = VK_FALSE;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional

		// toto je celkove globalne nastavenie blendovania. Cize per framebuffer blendovanie + globalne konstatny (farby), s ktorymi sa blenduje
		VkPipelineColorBlendStateCreateInfo colorBlendingStateInfo = { };
		colorBlendingStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlendingStateInfo.logicOpEnable = VK_FALSE;  // zakazem blendovanie cez logicke operacie a tym padom, ze som ho nepovolil ani per framebuffer, tak je globalne zakazane
		colorBlendingStateInfo.logicOp = VK_LOGIC_OP_COPY; // Optional, Note that this will automatically disable the first method, as if you had set blendEnable to VK_FALSE for every attached framebuffer! 
		colorBlendingStateInfo.attachmentCount = 1;
		colorBlendingStateInfo.pAttachments = &colorBlendAttachment;
		colorBlendingStateInfo.blendConstants[0] = 0.0f; // Optional
		colorBlendingStateInfo.blendConstants[1] = 0.0f; // Optional
		colorBlendingStateInfo.blendConstants[2] = 0.0f; // Optional
		colorBlendingStateInfo.blendConstants[3] = 0.0f; // Optional

		// pipeline layout definuje nastavenie uniform premennych
		VkPipelineLayoutCreateInfo pipelineLayoutInfo = { };
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 0; // Optional
		pipelineLayoutInfo.pSetLayouts = nullptr; // Optional
		pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
		pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

		VkResult res = vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout);
		if (res != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create pipeline layout!");
		}

		// ======
		// ====== TOTO UZ JE SAMOTNA PIPELINE SO VSETKYMI POTREBNYMI INFO (SHADER STAGES, FIXED-FUNCTION STATE, UNIFORMS/PIPELINE LAYOUT, RENDER PASS)
		VkGraphicsPipelineCreateInfo pipelineInfo = {};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = 2;
		pipelineInfo.pStages = shaderStages;
		pipelineInfo.pVertexInputState = &vertexInputInfo;
		pipelineInfo.pInputAssemblyState = &inputAssemblyInfo;
		pipelineInfo.pViewportState = &viewportStateInfo;
		pipelineInfo.pRasterizationState = &rasterizerStateInfo;
		pipelineInfo.pMultisampleState = &multisamplingStateInfo;
		pipelineInfo.pDepthStencilState = nullptr; // Optional
		pipelineInfo.pColorBlendState = &colorBlendingStateInfo;
		pipelineInfo.pDynamicState = nullptr; // Optional
		pipelineInfo.layout = m_pipelineLayout;
		pipelineInfo.renderPass = m_renderPass;
		// The index of the sub pass where this graphics pipeline will be used.
		// It is also possible to use other render passes with this pipeline instead of this specific instance, but they have to be compatible with renderPass
		pipelineInfo.subpass = 0;
		// Vulkan allows you to create a new graphics pipeline by deriving from an existing pipeline.
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
		pipelineInfo.basePipelineIndex = -1; // Optional

		res = vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_graphicsPipeline);
		if (res != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create graphics pipeline!");
		}

		// po skompilovani pipeline objectu, uz shader module nebudem potrebovat a mozem ho dealokovat
		vkDestroyShaderModule(m_device, fragShaderModule, nullptr);
		vkDestroyShaderModule(m_device, vertShaderModule, nullptr);
	}

	VkShaderModule createShaderModule(const std::vector<char>& code)
	{
		VkShaderModuleCreateInfo createInfo = { };
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = code.size();
		createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

		VkShaderModule shaderModule;
		VkResult res = vkCreateShaderModule(m_device, &createInfo, nullptr, &shaderModule);
		if (res != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create shader module!");
		}

		return shaderModule;
	}

	void createRenderPass()
	{
		VkAttachmentDescription colorAttachment = { };
		colorAttachment.format = m_swapChainImageFormat;
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		// The loadOp and storeOp determine what to do with the data in the attachment before rendering and after rendering.
		// The loadOp and storeOp apply to color and depth data
		//  VK_ATTACHMENT_LOAD_OP_LOAD: Preserve the existing contents of the attachment
		//  VK_ATTACHMENT_LOAD_OP_CLEAR : Clear the values to a constant at the start
		//  VK_ATTACHMENT_LOAD_OP_DONT_CARE : Existing contents are undefined; we don't care about them
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		//  VK_ATTACHMENT_STORE_OP_STORE: Rendered contents will be stored in memory and can be read later
		//  VK_ATTACHMENT_STORE_OP_DONT_CARE: Contents of the framebuffer will be undefined after the rendering
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		// stencilLoadOp / stencilStoreOp apply to stencil data
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		// The initialLayout specifies which layout the image will have before the render pass begins.
		// The finalLayout specifies the layout to automatically transition to when the render pass finishes.
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference colorAttachmentRef = { };
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass = { };
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;

		VkSubpassDependency dependency = { };
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		VkRenderPassCreateInfo renderPassInfo = { };
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = 1;
		renderPassInfo.pAttachments = &colorAttachment;
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;

		VkResult res = vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_renderPass);
		if (res != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create render pass!");
		}
	}

	void createFramebuffers()
	{
		m_swapChainFramebuffers.resize(m_swapChainImageViews.size());

		const size_t n = m_swapChainImageViews.size();
		for (size_t i = 0; i < n; ++i)
		{
			VkImageView attachments[] =
			{
				m_swapChainImageViews[i]
			};

			VkFramebufferCreateInfo framebufferInfo = { };
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = m_renderPass;
			framebufferInfo.attachmentCount = 1;
			framebufferInfo.pAttachments = attachments;
			framebufferInfo.width = m_swapChainExtent.width;
			framebufferInfo.height = m_swapChainExtent.height;
			framebufferInfo.layers = 1;

			VkResult res = vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, &m_swapChainFramebuffers[i]);
			if (res != VK_SUCCESS)
			{
				throw std::runtime_error("Failed to create framebuffer!");
			}
		}
	}

	void createCommandPool()
	{
		VkCommandPoolCreateInfo poolInfo = { };
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.queueFamilyIndex = m_graphicsQueueFamilyIndex;
		// VK_COMMAND_POOL_CREATE_TRANSIENT_BIT: Hint that command buffers are rerecorded with new commands very often(may change memory allocation behavior)
		// VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT : Allow command buffers to be rerecorded individually, without this flag they all have to be reset together
		poolInfo.flags = 0; // Optional

		VkResult res = vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool);
		if (res != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create command pool!");
		}
	}

	void createCommandBuffers()
	{
		m_commandBuffers.resize(m_swapChainFramebuffers.size());

		// *******
		// *** ALOKACIA POTREBNEHO POCTU COMMAND BUFFEROV
		VkCommandBufferAllocateInfo allocInfo = { };
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = m_commandPool;
		// The level parameter specifies if the allocated command buffers are primary or secondary command buffers.
		//   VK_COMMAND_BUFFER_LEVEL_PRIMARY   :  Can be submitted to a queue for execution, but cannot be called from other command buffers.
		//   VK_COMMAND_BUFFER_LEVEL_SECONDARY :  Cannot be submitted directly, but can be called from primary command buffers.
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = (uint32_t) m_commandBuffers.size();

		VkResult res = vkAllocateCommandBuffers(m_device, &allocInfo, m_commandBuffers.data());
		if (res != VK_SUCCESS)
		{
			throw std::runtime_error("failed to allocate command buffers!");
		}

		// *******
		// *** NAHRATIE PRIKAZOV DO COMMAND BUFFEROV, TU V PODSTATE DEFINUJEM AKE OPERACIE SA MAJU VYKONAT, ABY SA MI VYRENDEROVALO TO CO POTREBUJEM
		for (size_t i = 0; i < m_commandBuffers.size(); ++i)
		{
			VkResult res = VK_SUCCESS;

			VkCommandBufferBeginInfo beginInfo = { };
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
			beginInfo.pInheritanceInfo = nullptr; // Optional, only relevant for secondary command buffers

			// If the command buffer was already recorded once, then a call to vkBeginCommandBuffer will implicitly reset it.
			// It's not possible to append commands to a buffer at a later time.
			res = vkBeginCommandBuffer(m_commandBuffers[i], &beginInfo);
			if (res != VK_SUCCESS)
			{
				throw std::runtime_error("Failed to begin recording command buffer!");
			}

			VkRenderPassBeginInfo renderPassInfo = { };
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderPassInfo.renderPass = m_renderPass;
			renderPassInfo.framebuffer = m_swapChainFramebuffers[i];
			renderPassInfo.renderArea.offset = { 0, 0 };
			renderPassInfo.renderArea.extent = m_swapChainExtent;

			// The last two parameters define the clear values to use for VK_ATTACHMENT_LOAD_OP_CLEAR, which we used as load operation for the color attachment.
			VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
			renderPassInfo.clearValueCount = 1;
			renderPassInfo.pClearValues = &clearColor;

			// The final parameter controls how the drawing commands within the render pass will be provided.It can have one of two values :
			//   VK_SUBPASS_CONTENTS_INLINE                    :  The render pass commands will be embedded in the primary command buffer itself and no secondary command buffers will be executed.
			//   VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS :  The render pass commands will be executed from secondary command buffers.
			vkCmdBeginRenderPass(m_commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

			vkCmdBindPipeline(m_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);

			vkCmdDraw(m_commandBuffers[i], 3, 1, 0, 0);

			vkCmdEndRenderPass(m_commandBuffers[i]);

			res = vkEndCommandBuffer(m_commandBuffers[i]);
			if (res != VK_SUCCESS)
			{
				throw std::runtime_error("Failed to record command buffer!");
			}
		}
	}

	void createSyncObjects()
	{
		m_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		m_renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

		VkSemaphoreCreateInfo semaphoreInfo = { };
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fenceInfo = { };
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		// aby nedoslo k deadlocku hned na zaciatku prveho frejmu, lebo by som cakal na fence, ktory nebol nikdy signalizovany
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		{
			VkResult res = vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]);
			if (res != VK_SUCCESS)
			{
				throw std::runtime_error("Failed to create image available semaphore !");
			}

			res = vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]);
			if (res != VK_SUCCESS)
			{
				throw std::runtime_error("Failed to create render finished semaphore !");
			}

			res = vkCreateFence(m_device, &fenceInfo, nullptr, &m_inFlightFences[i]);
			if (res != VK_SUCCESS)
			{
				throw std::runtime_error("Failed to create render finished semaphore !");
			}
		}
	}
#pragma endregion

	// ///////////////////////////////////////////////////////////////////////////////////////////////
	// resizing
	// ///////////////////////////////////////////////////////////////////////////////////////////////

	void cleanupSwapChain()
	{
		vkFreeCommandBuffers(m_device, m_commandPool, static_cast<uint32_t>(m_commandBuffers.size()), m_commandBuffers.data());

		for (VkFramebuffer framebuffer : m_swapChainFramebuffers)
		{
			vkDestroyFramebuffer(m_device, framebuffer, nullptr);
		}

		vkDestroyPipeline(m_device, m_graphicsPipeline, nullptr);

		vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);

		vkDestroyRenderPass(m_device, m_renderPass, nullptr);

		for (VkImageView imageView : m_swapChainImageViews)
		{
			vkDestroyImageView(m_device, imageView, nullptr);
		}

		vkDestroySwapchainKHR(m_device, m_swapChain, nullptr);
	}

	void recreateSwapChain()
	{
		std::cout << "=== Swapchain cleanup - BEGIN ================================================================" << std::endl;

		// There is another case where a swap chain may become out of data and that is a special kind of window resizing : window minimization.
		// This case is special because it will result in a frame buffer size of 0.
		// This is handled by pausing until the window is in the foreground again.
		int width = 0, height = 0;
		while (width == 0 || height == 0)
		{
			glfwGetFramebufferSize(m_window, &width, &height);
			glfwWaitEvents();
		}

		vkDeviceWaitIdle(m_device);

		cleanupSwapChain();

		createSwapChain();
		createImageViews();
		createRenderPass();
		createGraphicsPipeline();
		createFramebuffers();
		createCommandBuffers();

		std::cout << "=== Swapchain cleanup - END ================================================================" << std::endl;
	}

	// ///////////////////////////////////////////////////////////////////////////////////////////////
	// main loop
	// ///////////////////////////////////////////////////////////////////////////////////////////////

	void mainLoop()
	{
		while (!glfwWindowShouldClose(m_window))
		{
			glfwPollEvents();
			drawFrame();
		}

		// caka, kym budu signalizovane vsetky zaqueue-ovane operacie, pretoze nemozem dealokovat semafor
		// na ktory este nejaky prikaz caka. Musim najprv pockat kym sa vsetky operacie dokoncia.
		vkDeviceWaitIdle(m_device);
	}

	void drawFrame()
	{
		VkResult res = VK_SUCCESS;

		//****
		//*** Sync CPU with GPU and wait for the last submitted frame to be finished

		// The VK_TRUE we pass here indicates that we want to wait for all fences. Last parameter is the timeout.
		vkWaitForFences(m_device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, std::numeric_limits<uint64_t>::max());

		//****
		//*** acquire the first swap chain image
		uint32_t imageIndex;
		// one does not have to wait before calling vkAcquireNextImageKHR, it will automatically wait for an image to be available.
		// Tato horna veta teda asi znamena aj to, ze netreba cakat na vkQueuePresentKHR kym sa dokonci, lebo to sa pred zacatim noveho frejmu deje automaticky
		res = vkAcquireNextImageKHR(m_device, m_swapChain, std::numeric_limits<uint64_t>::max(), m_imageAvailableSemaphores[m_currentFrame], VK_NULL_HANDLE, &imageIndex);
		if (res == VK_ERROR_OUT_OF_DATE_KHR)
		{
			// VK_ERROR_OUT_OF_DATE_KHR: The swap chain has become incompatible with the surface and can no longer be used for rendering. Usually happens after a window resize.
			recreateSwapChain();
			return;
		}
		else if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR)
		{
			// VK_SUBOPTIMAL_KHR : The swap chain can still be used to successfully present to the surface, but the surface properties are no longer matched exactly.
			throw std::runtime_error("Failed to acquire swap chain image!");
		}

		// unlike the semaphores, we manually need to restore the fence to the unsignaled state by resetting it
		// this needs to happen only after we early bail out of unsuccessfull swapchain recreation. Oterwise the fence would
		// never come to a signalled state (we would never reach the following vkQueueSubmit) and thus the app would deadlock.
		vkResetFences(m_device, 1, &m_inFlightFences[m_currentFrame]);

		//****
		//*** submit a drawing command, to draw the scene to the acquired swap chain image
		VkSemaphore waitSemaphores[] = { m_imageAvailableSemaphores[m_currentFrame] };
		// urcuje, v ktorej casti pipeline sa ma cakat na zadane semafory (jedna bitova maska podmienok pre kazdy jeden semafor zvlast)
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		VkSemaphore signalSemaphores[] = { m_renderFinishedSemaphores[m_currentFrame] };

		VkSubmitInfo submitInfo = { };
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &m_commandBuffers[imageIndex];
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		res = vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_inFlightFences[m_currentFrame]);
		if (res != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to submit draw command buffer!");
		}

		//****
		//*** Submit a command to display the rendered result on the screen
		VkSwapchainKHR swapChains[] = { m_swapChain };

		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;
		// specifies the swap chains to present images to
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;
		// and the index of the image for each swap chain.
		presentInfo.pImageIndices = &imageIndex;
		// allows to specify an array of VkResult values to check for every individual swap chain if presentation was successful.
		// It's not necessary if you're only using a single swap chain, because you can simply use the return value of the present function.
		presentInfo.pResults = nullptr; // Optional

		res = vkQueuePresentKHR(m_presentationQueue, &presentInfo);
		if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR || m_framebufferResized)
		{
			recreateSwapChain();
			m_framebufferResized = false;
		}
		else if (res != VK_SUCCESS)
		{
			throw std::runtime_error("failed to present swap chain image!");
		}

		// sync CPU with GPU (this however is not a very efficient way)
		//vkQueueWaitIdle(m_presentationQueue);

		m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	}

	// ///////////////////////////////////////////////////////////////////////////////////////////////
	// VULKAN cleanup
	// ///////////////////////////////////////////////////////////////////////////////////////////////

	void cleanup()
	{
		for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			vkDestroySemaphore(m_device, m_renderFinishedSemaphores[i], nullptr);
			vkDestroySemaphore(m_device, m_imageAvailableSemaphores[i], nullptr);
			vkDestroyFence(m_device, m_inFlightFences[i], nullptr);
		}

		vkDestroyCommandPool(m_device, m_commandPool, nullptr);

		// We should delete the framebuffers before the image views and render pass that they are based on, but only after we've finished rendering
		for (VkFramebuffer framebuffer : m_swapChainFramebuffers)
		{
			vkDestroyFramebuffer(m_device, framebuffer, nullptr);
		}

		vkDestroyPipeline(m_device, m_graphicsPipeline, nullptr);

		vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);

		vkDestroyRenderPass(m_device, m_renderPass, nullptr);

		// Unlike images, the image views were explicitly created by us, so we need to destroy them at the end of the program.
		for (VkImageView imageView : m_swapChainImageViews)
		{
			vkDestroyImageView(m_device, imageView, nullptr);
		}

		// treba zavolat este predtym ako je zniceny device
		vkDestroySwapchainKHR(m_device, m_swapChain, nullptr);

		// tato funkcia vycisti aj vsetky command queues
		vkDestroyDevice(m_device, nullptr);

		// surface needs to be destroyed before destroying the instace !!!
		vkDestroySurfaceKHR(m_instance, m_surface, nullptr);

		if (g_enableValidationLayers)
		{
			DestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
		}

		// All of the other Vulkan resources that we'll create in the following chapters should be cleaned up before the instance is destroyed.
		// druhy parameter su allocation callbacks
		vkDestroyInstance(m_instance, nullptr);

		glfwDestroyWindow(m_window);

		glfwTerminate();
	}

private:
	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData)
	{
		const char* sev = "";

		if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
		{
			sev = "VERBOSE";
		}
		else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
		{
			sev = "INFO";
		}
		else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
		{
			sev = "WARNING";
		}
		else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
		{
			sev = "ERROR";
		}

		std::cerr << "validation layer: " << sev << ": " << pCallbackData->pMessage << std::endl;

		// Ak by som vratil VK_TRUE, tak by to znamennalo, ze Vulkan call, z ktoreho bol zavolany tento callback bude terminovany
		// VK_TRUE sa pouziva viac-menej len na ladenie validation layers
		return VK_FALSE;
	}

	static void framebufferResizeCallback(GLFWwindow* window, int width, int height)
	{
		HelloTriangleApplication* app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window));
		app->m_framebufferResized = true;
	}

private:
	GLFWwindow* m_window = nullptr;

	// handle to Vulkan instance, Vulkan instance stores all of the Vulkan state of the application
	VkInstance m_instance = VK_NULL_HANDLE;

	// relays debug messages from validation layers to the application
	VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;

	// this is basically the render target, the window to which we will draw
	VkSurfaceKHR m_surface = VK_NULL_HANDLE;

	// handle to the physical device (e.g. the graphics card)
	VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;

	// which queue family of the given physical device to use
	uint32_t m_graphicsQueueFamilyIndex = -1;

	// which queue of the given physical device to use for presenting rendered results on the screen
	uint32_t m_presentationQueueFamilyIndex = -1;

	// logical device, abstracts the actual physical device
	// there can even be multiple logical devices per single physical device
	VkDevice m_device = VK_NULL_HANDLE;

	// queue, cez ktoru budem GPU-cku posielat vsetky prikazy (renderovanie, transport dat, ...)
	VkQueue m_graphicsQueue = VK_NULL_HANDLE;

	// a queue, that can process commands to diplay rendered contents on the screen
	// (this will most likely be the same queue as the graphics queue)
	VkQueue m_presentationQueue = VK_NULL_HANDLE;

	// swap chain, teda subor obrazkov, ktore budu prezentovane na obrazovke + nastavenia podmienok pre toto prezentovanie
	VkSwapchainKHR m_swapChain = VK_NULL_HANDLE;

	// handles to images of the swap chain, these images are freed when the swapchain is destroyed, there I do not need to care about that
	std::vector<VkImage> m_swapChainImages;

	VkFormat m_swapChainImageFormat;
	VkExtent2D m_swapChainExtent;

	// aby som vo Vulkane mohol pouzivat nejaky image, potrebujem si najprv vytvorit image view
	// ImageView describes how to access the image and which part of the image to access,
	// for example if it should be treated as a 2D texture depth texture without any mipmapping levels.
	// ImageView mi vlastne umoznuje pouzit image ako texturu.
	std::vector<VkImageView> m_swapChainImageViews;

	std::vector<VkFramebuffer> m_swapChainFramebuffers;

	// popisuje mi jednotlive render passes. Ake typy attachementov na ne budem potrebovat,
	// kedy maju byt tieto attachementy vyclearovane, aky maju format ...
	// Teda: the attachments referenced by the pipeline stages and their usage
	VkRenderPass m_renderPass = VK_NULL_HANDLE;

	// graficka pipeline, cize vsetok state + shadre + vsetko dalsie, co budem potrebovat pri jednom konkretnom render passe
	VkPipeline m_graphicsPipeline = VK_NULL_HANDLE;

	// udrzuje v sebe hodnoty pre jednotlive uniforms, ktore v shaderi mam
	VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;

	VkCommandPool m_commandPool = VK_NULL_HANDLE;

	// command buffers su automaticky dealokovane pri dealokacii command pool,
	// takze ja ich nemusim explicitne dealokovat
	std::vector<VkCommandBuffer> m_commandBuffers;

	// semaphores to synchronize operations executed by the graphics and presentation queues
	//
	// There are two ways of synchronizing swap chain events: fences and semaphores.
	// They're both objects that can be used for coordinating operations by having one operation signal and
	// another operation wait for a fence or semaphore to go from the unsignaled to signaled state.
	// 
	// The difference is that the state of fences can be accessed from your program using calls like vkWaitForFences and
	// semaphores cannot be.Fences are mainly designed to synchronize your application itself with rendering operation,
	// whereas semaphores are used to synchronize operations within or across command queues.We want to synchronize
	// the queue operations of draw commands and presentation, which makes semaphores the best fit.
	std::vector<VkSemaphore> m_imageAvailableSemaphores;
	std::vector<VkSemaphore> m_renderFinishedSemaphores;

	// to synchronize between GPU and CPU, when the frame is done rendering by GPU, then we have to wait for it in the CPU
	std::vector<VkFence> m_inFlightFences;

	// ktory frame prave renderujem, aby som vedel, ktory par semaforov mam pouzit
	size_t m_currentFrame = 0;

	// Although many drivers and platforms trigger VK_ERROR_OUT_OF_DATE_KHR automatically after a window resize,
	// it is not guaranteed to happen, hence this flag ...
	bool m_framebufferResized = false;
};


int main()
{
	HelloTriangleApplication app;

	try
	{
		app.run();
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}