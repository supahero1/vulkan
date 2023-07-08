#include "../include/vulkan.h"
#include "../include/debug.h"
#include "../include/util.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <cglm/cglm.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include <string.h>


static GLFWwindow* Window;


static const char* vkInstanceExtensions[] =
{
#ifndef NDEBUG
	VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif
};

static const char* vkLayers[] =
{
	"VK_LAYER_KHRONOS_validation"
};

static VkInstance vkInstance;


static VkSurfaceKHR vkSurface;
static VkSurfaceCapabilitiesKHR vkSurfaceCapabilities;


static const char* vkDeviceExtensions[] =
{
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

static uint32_t vkQueueID;
static VkQueue vkQueue;


static VkDevice vkDevice;
static VkExtent2D vkExtent;
static VkPhysicalDeviceLimits vkLimits;
static uint32_t vkMinImageCount;
static VkSurfaceTransformFlagBitsKHR vkTransform;
static VkPhysicalDeviceMemoryProperties vkMemoryProperties;


static VkSwapchainKHR vkSwapchain;
static VkImage* vkImages;
static VkImageView* vkImageViews;
static uint32_t vkImageCount;


static VkViewport vkViewport;
static VkRect2D vkScissor;


static VkCommandPool vkCommandPool;
static VkCommandBuffer vkCommandBuffer;
static VkFence vkFence;
static VkBuffer vkCopyBuffer;
static VkDeviceMemory vkCopyBufferMemory;


static VkSampler vkSampler;

typedef struct Image
{
	VkImage Image;
	VkImageView View;
	VkDeviceMemory Memory;
}
Image;

static Image vkTexture;
static Image vkDepthBuffer;


typedef struct VkVertexInput
{
	vec3 Position;
	vec2 Texel;
}
VkVertexInput;

static const VkVertexInput vkVertexInput[] =
{
	{{0.5f, 0.5f, 0.0f}, {1.0f, 1.0f}},
	{{-0.5f, 0.5f, 0.0f}, {0.0f, 1.0f}},
    {{0.5f, -0.5f, 0.0f}, {1.0f, 0.0f}},
	{{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f}},
	{{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f}},

	{{0.5f, 0.5f, -0.5f}, {1.0f, 1.0f}},
	{{0.5f, 0.5f, -0.5f}, {1.0f, 1.0f}},
	{{-0.5f, 0.5f, -0.5f}, {0.0f, 1.0f}},
    {{0.5f, -0.5f, -0.5f}, {1.0f, 0.0f}},
	{{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f}},
};

static VkBuffer vkVertexBuffer;
static VkDeviceMemory vkVertexBufferMemory;


typedef struct VkVertexUniformInput
{
	mat4 Model;
	mat4 View;
	mat4 Projection;
}
VkVertexUniformInput;


static VkDescriptorSetLayout vkDescriptors;
static VkRenderPass vkRenderPass;
static VkPipelineLayout vkPipelineLayout;
static VkPipeline vkPipeline;
static VkFramebuffer* vkFramebuffers;
static VkDescriptorPool vkDescriptorPool;


typedef enum Semaphore
{
	SEMAPHORE_IMAGE_AVAILABLE,
	SEMAPHORE_RENDER_FINISHED,
	kSEMAPHORE
}
Semaphore;

typedef enum Fence
{
	FENCE_IN_FLIGHT,
	kFENCE
}
Fence;

typedef struct VkFrame
{
	VkCommandBuffer CommandBuffer;
	VkSemaphore Semaphores[kSEMAPHORE];
	VkFence Fences[kFENCE];

	VkBuffer UniformBuffer;
	VkDeviceMemory UniformBufferMemory;
	void* UniformData;

	VkDescriptorSet DescriptorSet;
}
VkFrame;

static VkFrame* vkFrames;
static VkFrame* vkFrame;
static VkFrame* vkFrameEnd;


#ifndef NDEBUG

static VkDebugUtilsMessengerEXT DebugMessenger;

static VKAPI_ATTR VkBool32 VKAPI_CALL
VulkanDebugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT Severity,
	VkDebugUtilsMessageTypeFlagsEXT Type,
	const VkDebugUtilsMessengerCallbackDataEXT* Data,
	void* UserData
	)
{
	puts(Data->pMessage);

	// if(Severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
	// {
	// 	abort();
	// }

	return VK_FALSE;
}


static void*
VulkanGetProcAddr(
	const char* Name
	)
{
	return vkGetInstanceProcAddr(vkInstance, Name);
}


static VkResult
VulkanCreateDebugUtilsMessengerEXT(
	const VkDebugUtilsMessengerCreateInfoEXT* DebugInfo
	)
{
    PFN_vkCreateDebugUtilsMessengerEXT Function = VulkanGetProcAddr("vkCreateDebugUtilsMessengerEXT");

    if(Function != NULL)
	{
        return Function(vkInstance, DebugInfo, NULL, &DebugMessenger);
    }

    return VK_ERROR_EXTENSION_NOT_PRESENT;
}


static void
VulkanDestroyDebugUtilsMessengerEXT(
	void
	)
{
    PFN_vkDestroyDebugUtilsMessengerEXT Function = VulkanGetProcAddr("vkDestroyDebugUtilsMessengerEXT");

	if(Function != NULL)
	{
        Function(vkInstance, DebugMessenger, NULL);
    }
}

#endif /* NDEBUG */


static void
VulkanInitGLFW(
	void
	)
{
	int Success = glfwInit();
	AssertEQ(Success, GLFW_TRUE);

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);

	GLFWmonitor* Monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* VideoMode = glfwGetVideoMode(Monitor);

	Window = glfwCreateWindow(VideoMode->width, VideoMode->height, "2dpag", Monitor, NULL);
	AssertNEQ(Window, NULL);
}


static void
VulkanDestroyGLFW(
	void
	)
{
	glfwDestroyWindow(Window);
	glfwTerminate();
}


static void
VulkanInitInstance(
	void
	)
{
	VkResult Result;

	VkApplicationInfo AppInfo = {0};
	AppInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	AppInfo.pNext = NULL;
	AppInfo.pApplicationName = NULL;
	AppInfo.applicationVersion = 0;
	AppInfo.pEngineName = NULL;
	AppInfo.engineVersion = 0;
	AppInfo.apiVersion = VK_API_VERSION_1_3;

	VkInstanceCreateInfo CreateInfo = {0};
	CreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	CreateInfo.flags = 0;
	CreateInfo.pApplicationInfo = &AppInfo;

	uint32_t GLFWExtensionCount;
	const char** GLFWExtensions = glfwGetRequiredInstanceExtensions(&GLFWExtensionCount);

#ifndef NDEBUG
	uint32_t ExtensionCount = GLFWExtensionCount + ARRAYLEN(vkInstanceExtensions);
	const char* Extensions[ExtensionCount];

	const char** Extension = Extensions;
	for(uint32_t i = 0; i < GLFWExtensionCount; ++i)
	{
		*(Extension++) = GLFWExtensions[i];
	}
	for(uint32_t i = 0; i < ARRAYLEN(vkInstanceExtensions); ++i)
	{
		*(Extension++) = vkInstanceExtensions[i];
	}

	CreateInfo.enabledExtensionCount = ExtensionCount;
	CreateInfo.ppEnabledExtensionNames = Extensions;
#else
	CreateInfo.enabledExtensionCount = GLFWExtensionCount;
	CreateInfo.ppEnabledExtensionNames = GLFWExtensions;
#endif

#ifndef NDEBUG
	CreateInfo.enabledLayerCount = ARRAYLEN(vkLayers);
	CreateInfo.ppEnabledLayerNames = vkLayers;

	VkDebugUtilsMessengerCreateInfoEXT DebugInfo = {0};
	DebugInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	DebugInfo.pNext = NULL;
	DebugInfo.flags = 0;
	DebugInfo.messageSeverity =
		// VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	DebugInfo.messageType =
		VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	DebugInfo.pfnUserCallback = VulkanDebugCallback;
	DebugInfo.pUserData = NULL;

	CreateInfo.pNext = &DebugInfo;
#else
	CreateInfo.pNext = NULL;
#endif

	Result = vkCreateInstance(&CreateInfo, NULL, &vkInstance);
	AssertEQ(Result, VK_SUCCESS);

#ifndef NDEBUG
	Result = VulkanCreateDebugUtilsMessengerEXT(&DebugInfo);
	AssertEQ(Result, VK_SUCCESS);
#endif
}


static void
VulkanDestroyInstance(
	void
	)
{
	VulkanDestroyDebugUtilsMessengerEXT();
	vkDestroyInstance(vkInstance, NULL);
}


static void
VulkanInitSurface(
	void
	)
{
	VkResult Result = glfwCreateWindowSurface(vkInstance, Window, NULL, &vkSurface);
	AssertEQ(Result, VK_SUCCESS);
}


static void
VulkanDestroySurface(
	void
	)
{
	vkDestroySurfaceKHR(vkInstance, vkSurface, NULL);
}


typedef struct VkDeviceScore
{
	uint32_t Score;
	uint32_t QueueID;
	uint32_t MinImageCount;
	VkExtent2D Extent;
	VkSurfaceTransformFlagBitsKHR Transform;
	VkPhysicalDeviceLimits Limits;
}
VkDeviceScore;


static int
VulkanGetDeviceQueues(
	VkPhysicalDevice Device,
	VkDeviceScore* DeviceScore
	)
{
	uint32_t QueueCount;
	vkGetPhysicalDeviceQueueFamilyProperties(Device, &QueueCount, NULL);
	if(QueueCount == 0)
	{
		return 0;
	}

	VkQueueFamilyProperties Queues[QueueCount];
	vkGetPhysicalDeviceQueueFamilyProperties(Device, &QueueCount, Queues);

	VkQueueFamilyProperties* Queue = Queues;

	for(uint32_t i = 0; i < QueueCount; ++i, ++Queue)
	{
		VkBool32 Present;
		VkResult Result = vkGetPhysicalDeviceSurfaceSupportKHR(Device, i, vkSurface, &Present);
		AssertEQ(Result, VK_SUCCESS);

		if(Present && (Queue->queueFlags & VK_QUEUE_GRAPHICS_BIT))
		{
			DeviceScore->QueueID = i;

			return 1;
		}
	}

	return 0;
}


static int
VulkanGetDeviceExtensions(
	VkPhysicalDevice Device,
	VkDeviceScore* DeviceScore
	)
{
	if(ARRAYLEN(vkDeviceExtensions) == 0)
	{
		return 1;
	}

	uint32_t ExtensionCount;
	vkEnumerateDeviceExtensionProperties(Device, NULL, &ExtensionCount, NULL);
	if(ExtensionCount == 0)
	{
		return 0;
	}

	VkExtensionProperties Extensions[ExtensionCount];
	vkEnumerateDeviceExtensionProperties(Device, NULL, &ExtensionCount, Extensions);

	const char** RequiredExtension = vkDeviceExtensions;
	const char** RequiredExtensionEnd = vkDeviceExtensions + ARRAYLEN(vkDeviceExtensions);

	do
	{
		VkExtensionProperties* Extension = Extensions;
		VkExtensionProperties* EndExtension = Extensions + ExtensionCount;

		while(1)
		{
			if(strcmp(*RequiredExtension, Extension->extensionName) == 0)
			{
				break;
			}

			if(++Extension == EndExtension)
			{
				return 0;
			}
		}
	}
	while(++RequiredExtension != RequiredExtensionEnd);

	return 1;
}


static VkExtent2D
VulkanGetExtent(
	void
	)
{
	int Width = 0;
	int Height = 0;

	while(1)
	{
		glfwGetFramebufferSize(Window, &Width, &Height);

		if(Width != 0 && Height != 0)
		{
			break;
		}

		glfwWaitEvents();
	}

	Width = MAX(
		MIN(Width, vkSurfaceCapabilities.maxImageExtent.width),
		vkSurfaceCapabilities.minImageExtent.width
	);
	Height = MAX(
		MIN(Height, vkSurfaceCapabilities.maxImageExtent.height),
		vkSurfaceCapabilities.minImageExtent.height
	);

	return (VkExtent2D)
	{
		.width = Width,
		.height = Height
	};
}


static int
VulkanGetDeviceSwapChain(
	VkPhysicalDevice Device,
	VkDeviceScore* DeviceScore
	)
{
	uint32_t FormatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(Device, vkSurface, &FormatCount, NULL);
	if(FormatCount == 0)
	{
		return 0;
	}

	VkSurfaceFormatKHR Formats[FormatCount];
	vkGetPhysicalDeviceSurfaceFormatsKHR(Device, vkSurface, &FormatCount, Formats);

	VkSurfaceFormatKHR* Format = Formats;
	VkSurfaceFormatKHR* FormatEnd = Formats + ARRAYLEN(Formats);

	while(1)
	{
		if(Format->format == VK_FORMAT_B8G8R8A8_SRGB &&
			Format->colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			break;
		}

		if(++Format == FormatEnd)
		{
			return 0;
		}
	}


	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(Device, vkSurface, &vkSurfaceCapabilities);
	DeviceScore->Extent = VulkanGetExtent();

	DeviceScore->MinImageCount = MAX(
		MIN(3, vkSurfaceCapabilities.maxImageCount),
		vkSurfaceCapabilities.minImageCount
	);
	DeviceScore->Transform = vkSurfaceCapabilities.currentTransform;

	return 1;
}


static VkDeviceScore
VulkanGetDeviceScore(
	VkPhysicalDevice Device
	)
{
	VkDeviceScore DeviceScore = {0};

	VkPhysicalDeviceFeatures Features;
	vkGetPhysicalDeviceFeatures(Device, &Features);

	if(!Features.logicOp)
	{
		goto goto_err;
	}

	if(!Features.samplerAnisotropy)
	{
		goto goto_err;
	}

	if(!VulkanGetDeviceQueues(Device, &DeviceScore))
	{
		goto goto_err;
	}

	if(!VulkanGetDeviceExtensions(Device, &DeviceScore))
	{
		goto goto_err;
	}

	if(!VulkanGetDeviceSwapChain(Device, &DeviceScore))
	{
		goto goto_err;
	}

	VkPhysicalDeviceProperties Properties;
	vkGetPhysicalDeviceProperties(Device, &Properties);

	if(Properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
	{
		DeviceScore.Score += 1000;
	}

	DeviceScore.Score += Properties.limits.maxImageDimension2D;
	DeviceScore.Limits = Properties.limits;

	return DeviceScore;


	goto_err:

	DeviceScore.Score = 0;
	return DeviceScore;
}


static void
VulkanInitDevice(
	void
	)
{
	uint32_t DeviceCount;
	vkEnumeratePhysicalDevices(vkInstance, &DeviceCount, NULL);
	AssertNEQ(DeviceCount, 0);

	VkPhysicalDevice Devices[DeviceCount];
	vkEnumeratePhysicalDevices(vkInstance, &DeviceCount, Devices);

	VkPhysicalDevice* Device = Devices;
	VkPhysicalDevice* DeviceEnd = Devices + ARRAYLEN(Devices);

	VkPhysicalDevice BestDevice;
	VkDeviceScore BestDeviceScore = {0};

	do
	{
		VkDeviceScore ThisDeviceScore = VulkanGetDeviceScore(*Device);

		if(ThisDeviceScore.Score > BestDeviceScore.Score)
		{
			BestDevice = *Device;
			BestDeviceScore = ThisDeviceScore;
		}
	}
	while(++Device != DeviceEnd);

	AssertNEQ(BestDeviceScore.Score, 0);

	vkExtent = BestDeviceScore.Extent;
	vkQueueID = BestDeviceScore.QueueID;
	vkLimits = BestDeviceScore.Limits;


	float Priority = 1.0f;

	VkDeviceQueueCreateInfo Queue = {0};
	Queue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	Queue.pNext = NULL;
	Queue.flags = 0;
	Queue.queueFamilyIndex = vkQueueID;
	Queue.queueCount = 1;
	Queue.pQueuePriorities = &Priority;

	VkPhysicalDeviceFeatures DeviceFeatures = {0};
	DeviceFeatures.logicOp = VK_TRUE;
	DeviceFeatures.samplerAnisotropy = VK_TRUE;

	VkDeviceCreateInfo CreateInfo = {0};
	CreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	CreateInfo.pNext = NULL;
	CreateInfo.flags = 0;
	CreateInfo.queueCreateInfoCount = 1;
	CreateInfo.pQueueCreateInfos = &Queue;
	CreateInfo.enabledLayerCount = ARRAYLEN(vkLayers);
	CreateInfo.ppEnabledLayerNames = vkLayers;
	CreateInfo.enabledExtensionCount = ARRAYLEN(vkDeviceExtensions);
	CreateInfo.ppEnabledExtensionNames = vkDeviceExtensions;
	CreateInfo.pEnabledFeatures = &DeviceFeatures;

	VkResult Result = vkCreateDevice(BestDevice, &CreateInfo, NULL, &vkDevice);
	AssertEQ(Result, VK_SUCCESS);


	vkGetDeviceQueue(vkDevice, vkQueueID, 0, &vkQueue);

	vkMinImageCount = BestDeviceScore.MinImageCount;
	vkTransform = BestDeviceScore.Transform;

	vkGetPhysicalDeviceMemoryProperties(BestDevice, &vkMemoryProperties);
}


static void
VulkanDestroyDevice(
	void
	)
{
	vkDestroyDevice(vkDevice, NULL);
}


static void
VulkanCreateArbitraryImageView(
	VkImage Image,
	VkImageView* ImageView,
	VkFormat Format,
	VkImageAspectFlags Aspect
	)
{
	VkImageViewCreateInfo CreateInfo = {0};
	CreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	CreateInfo.pNext = NULL;
	CreateInfo.flags = 0;
	CreateInfo.image = Image;
	CreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	CreateInfo.format = Format;
	CreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	CreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	CreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	CreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	CreateInfo.subresourceRange.aspectMask = Aspect;
	CreateInfo.subresourceRange.baseMipLevel = 0;
	CreateInfo.subresourceRange.levelCount = 1;
	CreateInfo.subresourceRange.baseArrayLayer = 0;
	CreateInfo.subresourceRange.layerCount = 1;

	VkResult Result = vkCreateImageView(vkDevice, &CreateInfo, NULL, ImageView);
	AssertEQ(Result, VK_SUCCESS);
}


static void
VulkanCreateImageView(
	VkImage Image,
	VkImageView* ImageView
	)
{
	VulkanCreateArbitraryImageView(Image, ImageView, VK_FORMAT_B8G8R8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
}


static void
VulkanCreateDepthImageView(
	VkImage Image,
	VkImageView* ImageView
	)
{
	VulkanCreateArbitraryImageView(Image, ImageView, VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT);
}


static void
VulkanInitSwapchain(
	void
	)
{
	VkSwapchainCreateInfoKHR CreateInfo = {0};
	CreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	CreateInfo.pNext = NULL;
	CreateInfo.flags = 0;
	CreateInfo.surface = vkSurface;
	CreateInfo.minImageCount = vkMinImageCount;
	CreateInfo.imageFormat = VK_FORMAT_B8G8R8A8_SRGB;
	CreateInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	CreateInfo.imageExtent = vkExtent;
	CreateInfo.imageArrayLayers = 1;
	CreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	CreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	CreateInfo.queueFamilyIndexCount = 0;
	CreateInfo.pQueueFamilyIndices = NULL;
	CreateInfo.preTransform = vkTransform;
	CreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	CreateInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
	CreateInfo.clipped = VK_TRUE;
	CreateInfo.oldSwapchain = VK_NULL_HANDLE;

	VkResult Result = vkCreateSwapchainKHR(vkDevice, &CreateInfo, NULL, &vkSwapchain);
	AssertEQ(Result, VK_SUCCESS);


	vkGetSwapchainImagesKHR(vkDevice, vkSwapchain, &vkImageCount, NULL);
	AssertNEQ(vkImageCount, 0);

	vkImages = malloc(sizeof(*vkImages) * vkImageCount);
	AssertNEQ(vkImages, NULL);
	vkGetSwapchainImagesKHR(vkDevice, vkSwapchain, &vkImageCount, vkImages);

	VkImage* Image = vkImages;
	VkImage* ImageEnd = vkImages + vkImageCount;

	vkImageViews = malloc(sizeof(*vkImageViews) * vkImageCount);
	AssertNEQ(vkImageViews, NULL);

	VkImageView* ImageView = vkImageViews;

	while(1)
	{
		VulkanCreateImageView(*Image, ImageView);

		if(++Image == ImageEnd)
		{
			break;
		}

		++ImageView;
	}
}


static void
VulkanDestroySwapchain(
	void
	)
{
	VkImageView* ImageView = vkImageViews;
	VkImageView* ImageViewEnd = vkImageViews + vkImageCount;

	do
	{
		vkDestroyImageView(vkDevice, *ImageView, NULL);
	}
	while(++ImageView != ImageViewEnd);

	free(vkImageViews);
	free(vkImages);

	vkDestroySwapchainKHR(vkDevice, vkSwapchain, NULL);
}


static void
VulkanInitFrames(
	void
	)
{
	vkFrames = calloc(vkImageCount, sizeof(*vkFrames));
	AssertNEQ(vkFrames, NULL);

	vkFrame = vkFrames;
	vkFrameEnd = vkFrames + vkImageCount;
}


static void
VulkanDestroyFrames(
	void
	)
{
	free(vkFrames);
}


static void
VulkanInitCommands(
	void
	)
{
	VkCommandPoolCreateInfo PoolInfo = {0};
	PoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	PoolInfo.pNext = NULL;
	PoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	PoolInfo.queueFamilyIndex = vkQueueID;

	VkResult Result = vkCreateCommandPool(vkDevice, &PoolInfo, NULL, &vkCommandPool);
	AssertEQ(Result, VK_SUCCESS);


	VkCommandBuffer CommandBuffers[1 + vkImageCount];

	VkCommandBufferAllocateInfo AllocInfo = {0};
	AllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	AllocInfo.pNext = NULL;
	AllocInfo.commandPool = vkCommandPool;
	AllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	AllocInfo.commandBufferCount = ARRAYLEN(CommandBuffers);

	Result = vkAllocateCommandBuffers(vkDevice, &AllocInfo, CommandBuffers);
	AssertEQ(Result, VK_SUCCESS);

	vkCommandBuffer = CommandBuffers[0];

	VkFrame* Frame = vkFrames;
	VkCommandBuffer* CommandBuffer = CommandBuffers + 1;

	while(1)
	{
		Frame->CommandBuffer = *CommandBuffer;

		if(++Frame == vkFrameEnd)
		{
			break;
		}

		++CommandBuffer;
	}

	VkFenceCreateInfo FenceInfo = {0};
	FenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	FenceInfo.pNext = NULL;
	FenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	Result = vkCreateFence(vkDevice, &FenceInfo, NULL, &vkFence);
	AssertEQ(Result, VK_SUCCESS);
}


static void
VulkanDestroyCommands(
	void
	)
{
	vkDestroyFence(vkDevice, vkFence, NULL);


	VkCommandBuffer CommandBuffers[1 + vkImageCount];
	CommandBuffers[0] = vkCommandBuffer;

	VkFrame* Frame = vkFrames;

	VkCommandBuffer* CommandBuffer = CommandBuffers + 1;

	while(1)
	{
		*CommandBuffer = Frame->CommandBuffer;

		if(++Frame == vkFrameEnd)
		{
			break;
		}

		++CommandBuffer;
	}

	vkFreeCommandBuffers(vkDevice, vkCommandPool, ARRAYLEN(CommandBuffers), CommandBuffers);
	vkDestroyCommandPool(vkDevice, vkCommandPool, NULL);
}


static void
VulkanInitSampler(
	void
	)
{
	VkSamplerCreateInfo CreateInfo = {0};
	CreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	CreateInfo.pNext = NULL;
	CreateInfo.flags = 0;
	CreateInfo.magFilter = VK_FILTER_LINEAR;
	CreateInfo.minFilter = VK_FILTER_LINEAR;
	CreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	CreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	CreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	CreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	CreateInfo.mipLodBias = 0.0f;
	CreateInfo.anisotropyEnable = VK_TRUE;
	CreateInfo.maxAnisotropy = vkLimits.maxSamplerAnisotropy;
	CreateInfo.compareEnable = VK_FALSE;
	CreateInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	CreateInfo.minLod = 0.0f;
	CreateInfo.maxLod = 0.0f;
	CreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
	CreateInfo.unnormalizedCoordinates = VK_FALSE;

	VkResult Result = vkCreateSampler(vkDevice, &CreateInfo, NULL, &vkSampler);
	AssertEQ(Result, VK_SUCCESS);
}


static void
VulkanDestroySampler(
	void
	)
{
	vkDestroySampler(vkDevice, vkSampler, NULL);
}


static VkShaderModule
VulkanCreateShader(
	const char* Path
	)
{
	uint64_t Size;
	uint8_t* Buffer;

	int Error = ReadFile(Path, &Size, &Buffer);
	AssertEQ(Error, 0);

	const uint32_t* Code = (const uint32_t*) Buffer;

	VkShaderModuleCreateInfo CreateInfo = {0};
	CreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	CreateInfo.pNext = NULL;
	CreateInfo.flags = 0;
	CreateInfo.codeSize = Size;
	CreateInfo.pCode = Code;

	VkShaderModule Shader;

	VkResult Result = vkCreateShaderModule(vkDevice, &CreateInfo, NULL, &Shader);
	AssertEQ(Result, VK_SUCCESS);

	free(Buffer);
	return Shader;
}


static void
VulkanDestroyShader(
	VkShaderModule Shader
	)
{
	vkDestroyShaderModule(vkDevice, Shader, NULL);
}


static uint32_t
VulkanGetMemory(
	uint32_t Bits,
	VkMemoryPropertyFlags Properties
	)
{
	for(uint32_t i = 0; i < vkMemoryProperties.memoryTypeCount; ++i)
	{
		if(
			(Bits & (1 << i)) &&
			(vkMemoryProperties.memoryTypes[i].propertyFlags & Properties) == Properties
			)
		{
			return i;
		}
	}

	__builtin_unreachable();
}


static void
VulkanGetBuffer(
	VkDeviceSize Size,
	VkBufferUsageFlags Usage,
	VkMemoryPropertyFlags Properties,
	VkBuffer* Buffer,
	VkDeviceMemory* BufferMemory
	)
{
	VkBufferCreateInfo CreateInfo = {0};
	CreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	CreateInfo.pNext = NULL;
	CreateInfo.flags = 0;
	CreateInfo.size = Size;
	CreateInfo.usage = Usage;
	CreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	CreateInfo.queueFamilyIndexCount = 0;
	CreateInfo.pQueueFamilyIndices = NULL;

	VkResult Result = vkCreateBuffer(vkDevice, &CreateInfo, NULL, Buffer);
	AssertEQ(Result, VK_SUCCESS);

	VkMemoryRequirements Requirements;
	vkGetBufferMemoryRequirements(vkDevice, *Buffer, &Requirements);

	VkMemoryAllocateInfo AllocInfo = {0};
	AllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	AllocInfo.pNext = NULL;
	AllocInfo.allocationSize = Requirements.size;
	AllocInfo.memoryTypeIndex = VulkanGetMemory(Requirements.memoryTypeBits, Properties);

	Result = vkAllocateMemory(vkDevice, &AllocInfo, NULL, BufferMemory);
	AssertEQ(Result, VK_SUCCESS);

	vkBindBufferMemory(vkDevice, *Buffer, *BufferMemory, 0);
}


static void
VulkanGetStagingBuffer(
	VkDeviceSize Size,
	VkBuffer* Buffer,
	VkDeviceMemory* BufferMemory
	)
{
	VulkanGetBuffer(Size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
		VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, Buffer, BufferMemory);
}


static void
VulkanGetFinalBuffer(
	VkDeviceSize Size,
	VkBuffer* Buffer,
	VkDeviceMemory* BufferMemory
	)
{
	VulkanGetBuffer(Size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, Buffer, BufferMemory);
}


static void
VulkanBeginCommandBuffer(
	void
	)
{
	VkResult Result = vkWaitForFences(vkDevice, 1, &vkFence, VK_TRUE, UINT64_MAX);
	AssertEQ(Result, VK_SUCCESS);

	Result = vkResetFences(vkDevice, 1, &vkFence);
	AssertEQ(Result, VK_SUCCESS);

	Result = vkResetCommandBuffer(vkCommandBuffer, 0);
	AssertEQ(Result, VK_SUCCESS);

	VkCommandBufferBeginInfo BeginInfo = {0};
	BeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	BeginInfo.pNext = NULL;
	BeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	BeginInfo.pInheritanceInfo = NULL;

	Result = vkBeginCommandBuffer(vkCommandBuffer, &BeginInfo);
	AssertEQ(Result, VK_SUCCESS);
}


static void
VulkanEndCommandBuffer(
	void
	)
{
	VkResult Result = vkEndCommandBuffer(vkCommandBuffer);
	AssertEQ(Result, VK_SUCCESS);

	VkSubmitInfo SubmitInfo = {0};
	SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	SubmitInfo.pNext = NULL;
	SubmitInfo.waitSemaphoreCount = 0;
	SubmitInfo.pWaitSemaphores = NULL;
	SubmitInfo.pWaitDstStageMask = NULL;
	SubmitInfo.commandBufferCount = 1;
	SubmitInfo.pCommandBuffers = &vkCommandBuffer;
	SubmitInfo.signalSemaphoreCount = 0;
	SubmitInfo.pSignalSemaphores = NULL;

	vkQueueSubmit(vkQueue, 1, &SubmitInfo, vkFence);
}


static void
VulkanDestroyCopyBuffer(
	void
	)
{
	vkFreeMemory(vkDevice, vkCopyBufferMemory, NULL);
	vkDestroyBuffer(vkDevice, vkCopyBuffer, NULL);
}


static void
VulkanCopyToBuffer(
	VkBuffer Buffer,
	const void* Data,
	VkDeviceSize Size
	)
{
	VulkanBeginCommandBuffer();

	VulkanDestroyCopyBuffer();

	VulkanGetStagingBuffer(Size, &vkCopyBuffer, &vkCopyBufferMemory);

	void* Memory;

	VkResult Result = vkMapMemory(vkDevice, vkCopyBufferMemory, 0, VK_WHOLE_SIZE, 0, &Memory);
	AssertEQ(Result, VK_SUCCESS);

	memcpy(Memory, vkVertexInput, Size);
	vkUnmapMemory(vkDevice, vkCopyBufferMemory);

	VkBufferCopy Copy = {0};
	Copy.srcOffset = 0;
	Copy.dstOffset = 0;
	Copy.size = Size;

	vkCmdCopyBuffer(vkCommandBuffer, vkCopyBuffer, Buffer, 1, &Copy);

	VulkanEndCommandBuffer();
}


static void
VulkanCopyToImage(
	Image* Image,
	const void* Data,
	uint32_t Width,
	uint32_t Height
	)
{
	VulkanBeginCommandBuffer();

	VulkanDestroyCopyBuffer();

	VkDeviceSize Size = Width * Height * 4;

	VulkanGetStagingBuffer(Size, &vkCopyBuffer, &vkCopyBufferMemory);

	void* Memory;

	VkResult Result = vkMapMemory(vkDevice, vkCopyBufferMemory, 0, VK_WHOLE_SIZE, 0, &Memory);
	AssertEQ(Result, VK_SUCCESS);

	memcpy(Memory, Data, Size);
	vkUnmapMemory(vkDevice, vkCopyBufferMemory);

	VkBufferImageCopy Copy = {0};
	Copy.bufferOffset = 0;
	Copy.bufferRowLength = 0;
	Copy.bufferImageHeight = 0;
	Copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	Copy.imageSubresource.mipLevel = 0;
	Copy.imageSubresource.baseArrayLayer = 0;
	Copy.imageSubresource.layerCount = 1;
	Copy.imageOffset = (VkOffset3D){ 0, 0, 0 };
	Copy.imageExtent = (VkExtent3D){ Width, Height, 1 };

	vkCmdCopyBufferToImage(vkCommandBuffer, vkCopyBuffer, Image->Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &Copy);

	VulkanEndCommandBuffer();
}


static void
VulkanCreateImage(
	uint32_t Width,
	uint32_t Height,
	VkFormat Format,
	VkImageUsageFlags Usage,
	VkMemoryPropertyFlags Properties,
	Image* Image
	)
{
	VkImageCreateInfo ImageInfo = {0};
	ImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	ImageInfo.pNext = NULL;
	ImageInfo.flags = 0;
	ImageInfo.imageType = VK_IMAGE_TYPE_2D;
	ImageInfo.format = Format;
	ImageInfo.extent.width = Width;
	ImageInfo.extent.height = Height;
	ImageInfo.extent.depth = 1;
	ImageInfo.mipLevels = 1;
	ImageInfo.arrayLayers = 1;
	ImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	ImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	ImageInfo.usage = Usage;
	ImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	ImageInfo.queueFamilyIndexCount = 0;
	ImageInfo.pQueueFamilyIndices = NULL;
	ImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VkResult Result = vkCreateImage(vkDevice, &ImageInfo, NULL, &Image->Image);
	AssertEQ(Result, VK_SUCCESS);

	VkMemoryRequirements Requirements;
	vkGetImageMemoryRequirements(vkDevice, Image->Image, &Requirements);

	VkMemoryAllocateInfo AllocInfo = {0};
	AllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	AllocInfo.pNext = NULL;
	AllocInfo.allocationSize = Requirements.size;
	AllocInfo.memoryTypeIndex = VulkanGetMemory(Requirements.memoryTypeBits, Properties);

	Result = vkAllocateMemory(vkDevice, &AllocInfo, NULL, &Image->Memory);
	AssertEQ(Result, VK_SUCCESS);

	Result = vkBindImageMemory(vkDevice, Image->Image, Image->Memory, 0);
	AssertEQ(Result, VK_SUCCESS);
}


static void
VulkanDestroyImage(
	Image* Image
	)
{
	vkFreeMemory(vkDevice, Image->Memory, NULL);
	vkDestroyImageView(vkDevice, Image->View, NULL);
	vkDestroyImage(vkDevice, Image->Image, NULL);
}


static void
VulkanTransitionImageLayout(
	Image* Image,
	VkFormat Format,
	VkImageLayout From,
	VkImageLayout To
	)
{
	VulkanBeginCommandBuffer();

	VkPipelineStageFlags SourceStage;
	VkPipelineStageFlags DestinationStage;

	VkImageMemoryBarrier Barrier = {0};
	Barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	Barrier.pNext = NULL;
	Barrier.srcAccessMask = 0;
	Barrier.dstAccessMask = 0;

	if(From == VK_IMAGE_LAYOUT_UNDEFINED && To == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		Barrier.srcAccessMask = 0;
		Barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		SourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		DestinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if(From == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && To == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		Barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		Barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		SourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		DestinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else
	{
		AssertEQ(0, 1);
	}

	Barrier.oldLayout = From;
	Barrier.newLayout = To;
	Barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	Barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	Barrier.image = Image->Image;
	Barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	Barrier.subresourceRange.baseMipLevel = 0;
	Barrier.subresourceRange.levelCount = 1;
	Barrier.subresourceRange.baseArrayLayer = 0;
	Barrier.subresourceRange.layerCount = 1;

	vkCmdPipelineBarrier(vkCommandBuffer, SourceStage, DestinationStage, 0, 0, NULL, 0, NULL, 1, &Barrier);

	VulkanEndCommandBuffer();
}


static void
VulkanCreateTexture(
	const char* Path, // "textures/texture2.jpg"
	Image* Image
	)
{
	int TextureWidth;
	int TextureHeight;
	int TextureChannels;

	stbi_uc* Pixels = stbi_load(Path, &TextureWidth, &TextureHeight, &TextureChannels, STBI_rgb_alpha);
	AssertNEQ(Pixels, NULL);

	VulkanCreateImage(TextureWidth, TextureHeight, VK_FORMAT_B8G8R8A8_SRGB,
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, Image);

	VulkanTransitionImageLayout(Image, VK_FORMAT_B8G8R8A8_SRGB,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	VulkanCopyToImage(Image, Pixels, TextureWidth, TextureHeight);

	stbi_image_free(Pixels);

	VulkanTransitionImageLayout(Image, VK_FORMAT_B8G8R8A8_SRGB,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	VulkanCreateImageView(Image->Image, &Image->View);
}


static void
VulkanDestroyTexture(
	Image* Texture
	)
{
	VulkanDestroyImage(Texture);
}


static void
VulkanInitDepthBuffer(
	void
	)
{
	VulkanCreateImage(vkExtent.width, vkExtent.height, VK_FORMAT_D32_SFLOAT,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &vkDepthBuffer);
	VulkanCreateDepthImageView(vkDepthBuffer.Image, &vkDepthBuffer.View);
}


static void
VulkanDestroyDepthBuffer(
	void
	)
{
	VulkanDestroyImage(&vkDepthBuffer);
}


static void
VulkanInitFramebuffers(
	void
	)
{
	VkImageView* ImageView = vkImageViews;
	VkImageView* ImageViewEnd = vkImageViews + vkImageCount;

	vkFramebuffers = calloc(vkImageCount, sizeof(*vkFramebuffers));
	AssertNEQ(vkFramebuffers, NULL);

	VkFramebuffer* Framebuffer = vkFramebuffers;

	while(1)
	{
		VkImageView Attachments[] =
		{
			*ImageView,
			vkDepthBuffer.View
		};

		VkFramebufferCreateInfo CreateInfo = {0};
		CreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		CreateInfo.pNext = NULL;
		CreateInfo.flags = 0;
		CreateInfo.renderPass = vkRenderPass;
		CreateInfo.attachmentCount = ARRAYLEN(Attachments);
		CreateInfo.pAttachments = Attachments;
		CreateInfo.width = vkExtent.width;
		CreateInfo.height = vkExtent.height;
		CreateInfo.layers = 1;

		VkResult Result = vkCreateFramebuffer(vkDevice, &CreateInfo, NULL, Framebuffer);
		AssertEQ(Result, VK_SUCCESS);

		if(++ImageView == ImageViewEnd)
		{
			break;
		}

		++Framebuffer;
	}
}


static void
VulkanDestroyFramebuffers(
	void
	)
{
	VkFramebuffer* Framebuffer = vkFramebuffers;
	VkFramebuffer* FramebufferEnd = vkFramebuffers + vkImageCount;

	do
	{
		vkDestroyFramebuffer(vkDevice, *Framebuffer, NULL);
	}
	while(++Framebuffer != FramebufferEnd);

	free(vkFramebuffers);
}


static void
VulkanGetConsts(
	void
	)
{
	vkViewport.x = 0.0f;
	vkViewport.y = 0.0f;
	vkViewport.width = vkExtent.width;
	vkViewport.height = vkExtent.height;
	vkViewport.minDepth = 0.0f;
	vkViewport.maxDepth = 1.0f;

	vkScissor.offset.x = 0;
	vkScissor.offset.y = 0;
	vkScissor.extent = vkExtent;
}


static void
VulkanInitPipeline(
	void
	)
{
	VulkanGetConsts();

	VulkanCreateTexture("textures/texture2.jpg", &vkTexture);

	VkAttachmentReference ColorAttachmentRef = {0};
	ColorAttachmentRef.attachment = 0;
	ColorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference DepthAttachmentRef = {0};
	DepthAttachmentRef.attachment = 1;
	DepthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription Subpass = {0};
	Subpass.flags = 0;
	Subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	Subpass.inputAttachmentCount = 0;
	Subpass.pInputAttachments = NULL;
	Subpass.colorAttachmentCount = 1;
	Subpass.pColorAttachments = &ColorAttachmentRef;
	Subpass.pResolveAttachments = NULL;
	Subpass.pDepthStencilAttachment = &DepthAttachmentRef;
	Subpass.preserveAttachmentCount = 0;
	Subpass.pPreserveAttachments = NULL;

	VkSubpassDependency Dependency = {0};
	Dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	Dependency.dstSubpass = 0;
	Dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	Dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	Dependency.srcAccessMask = 0;
	Dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	Dependency.dependencyFlags = 0;

	VkAttachmentDescription Attachments[2] = {0};

	Attachments[0].flags = 0;
	Attachments[0].format = VK_FORMAT_B8G8R8A8_SRGB;
	Attachments[0].samples = VK_SAMPLE_COUNT_1_BIT; // multisampling here
	Attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	Attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	Attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	Attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	Attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	Attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	Attachments[1].flags = 0;
	Attachments[1].format = VK_FORMAT_D32_SFLOAT;
	Attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
	Attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	Attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	Attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	Attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	Attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	Attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkRenderPassCreateInfo RenderPassInfo = {0};
	RenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	RenderPassInfo.pNext = NULL;
	RenderPassInfo.flags = 0;
	RenderPassInfo.attachmentCount = ARRAYLEN(Attachments);
	RenderPassInfo.pAttachments = Attachments;
	RenderPassInfo.subpassCount = 1;
	RenderPassInfo.pSubpasses = &Subpass;
	RenderPassInfo.dependencyCount = 1;
	RenderPassInfo.pDependencies = &Dependency;

	VkResult Result = vkCreateRenderPass(vkDevice, &RenderPassInfo, NULL, &vkRenderPass);
	AssertEQ(Result, VK_SUCCESS);


	VkShaderModule VertexModule = VulkanCreateShader("bin/vert.spv");
	VkShaderModule FragmentModule = VulkanCreateShader("bin/frag.spv");

	VkPipelineShaderStageCreateInfo Stages[2] = {0};

	Stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	Stages[0].pNext = NULL;
	Stages[0].flags = 0;
	Stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	Stages[0].module = VertexModule;
	Stages[0].pName = "main";
	Stages[0].pSpecializationInfo = NULL;

	Stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	Stages[1].pNext = NULL;
	Stages[1].flags = 0;
	Stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	Stages[1].module = FragmentModule;
	Stages[1].pName = "main";
	Stages[1].pSpecializationInfo = NULL;

	VkDynamicState DynamicStates[] =
	{
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	VkPipelineDynamicStateCreateInfo DynamicState = {0};
	DynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	DynamicState.pNext = NULL;
	DynamicState.flags = 0;
	DynamicState.dynamicStateCount = ARRAYLEN(DynamicStates);
	DynamicState.pDynamicStates = DynamicStates;

	VkVertexInputBindingDescription VertexBinding = {0};
	VertexBinding.binding = 0;
	VertexBinding.stride = sizeof(VkVertexInput);
	VertexBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VkVertexInputAttributeDescription Attributes[2] = {0};

	Attributes[0].location = 0;
	Attributes[0].binding = 0;
	Attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	Attributes[0].offset = offsetof(VkVertexInput, Position);

	Attributes[1].location = 1;
	Attributes[1].binding = 0;
	Attributes[1].format = VK_FORMAT_R32G32_SFLOAT;
	Attributes[1].offset = offsetof(VkVertexInput, Texel);

	VkPipelineVertexInputStateCreateInfo VertexInput = {0};
	VertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	VertexInput.pNext = NULL;
	VertexInput.flags = 0;
	VertexInput.vertexBindingDescriptionCount = 1;
	VertexInput.pVertexBindingDescriptions = &VertexBinding;
	VertexInput.vertexAttributeDescriptionCount = ARRAYLEN(Attributes);
	VertexInput.pVertexAttributeDescriptions = Attributes;

	VkPipelineInputAssemblyStateCreateInfo InputAssembly = {0};
	InputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	InputAssembly.pNext = NULL;
	InputAssembly.flags = 0;
	InputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	InputAssembly.primitiveRestartEnable = VK_FALSE;

	VkPipelineViewportStateCreateInfo ViewportState = {0};
	ViewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	ViewportState.pNext = NULL;
	ViewportState.flags = 0;
	ViewportState.viewportCount = 1;
	ViewportState.pViewports = &vkViewport; // ignored cuz dynamic
	ViewportState.scissorCount = 1;
	ViewportState.pScissors = &vkScissor; // ignored cuz dynamic

	VkPipelineRasterizationStateCreateInfo Rasterizer = {0};
	Rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	Rasterizer.pNext = NULL;
	Rasterizer.flags = 0;
	Rasterizer.depthClampEnable = VK_FALSE;
	Rasterizer.rasterizerDiscardEnable = VK_FALSE;
	Rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	Rasterizer.cullMode = VK_CULL_MODE_NONE; //VK_CULL_MODE_BACK_BIT;
	Rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	Rasterizer.depthBiasEnable = VK_FALSE;
	Rasterizer.depthBiasConstantFactor = 0.0f;
	Rasterizer.depthBiasClamp = 0.0f;
	Rasterizer.depthBiasSlopeFactor = 0.0f;
	Rasterizer.lineWidth = 1.0f;

	VkPipelineMultisampleStateCreateInfo Multisampling = {0};
	Multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	Multisampling.pNext = NULL;
	Multisampling.flags = 0;
	Multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	Multisampling.sampleShadingEnable = VK_FALSE;
	Multisampling.minSampleShading = 1.0f;
	Multisampling.pSampleMask = NULL;
	Multisampling.alphaToCoverageEnable = VK_FALSE;
	Multisampling.alphaToOneEnable = VK_FALSE;

	VkPipelineDepthStencilStateCreateInfo DepthStencil = {0};
	DepthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	DepthStencil.pNext = NULL;
	DepthStencil.flags = 0;
	DepthStencil.depthTestEnable = VK_TRUE;
	DepthStencil.depthWriteEnable = VK_TRUE;
	DepthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
	DepthStencil.depthBoundsTestEnable = VK_FALSE;
	DepthStencil.stencilTestEnable = VK_FALSE;
	DepthStencil.front = (VkStencilOpState){0};
	DepthStencil.back = (VkStencilOpState){0};
	DepthStencil.minDepthBounds = 0.0f;
	DepthStencil.maxDepthBounds = 0.0f;

	VkPipelineColorBlendAttachmentState BlendingAttachment = {0};
	BlendingAttachment.blendEnable = VK_FALSE; // VK_TRUE
	BlendingAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	BlendingAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	BlendingAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	BlendingAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	BlendingAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	BlendingAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
	BlendingAttachment.colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	VkPipelineColorBlendStateCreateInfo Blending = {0};
	Blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	Blending.pNext = NULL;
	Blending.flags = 0;
	Blending.logicOpEnable = VK_FALSE; // VK_TRUE
	Blending.logicOp = VK_LOGIC_OP_COPY;
	Blending.attachmentCount = 1;
	Blending.pAttachments = &BlendingAttachment;
	Blending.blendConstants[0] = 0.0f;
	Blending.blendConstants[1] = 0.0f;
	Blending.blendConstants[2] = 0.0f;
	Blending.blendConstants[3] = 0.0f;

	VkDescriptorSetLayoutBinding Bindings[2] = {0};

	Bindings[0].binding = 0;
	Bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	Bindings[0].descriptorCount = 1;
	Bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	Bindings[0].pImmutableSamplers = NULL;

	Bindings[1].binding = 1;
	Bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	Bindings[1].descriptorCount = 1;
	Bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	Bindings[1].pImmutableSamplers = NULL;

	VkDescriptorSetLayoutCreateInfo Descriptors = {0};
	Descriptors.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	Descriptors.pNext = NULL;
	Descriptors.flags = 0;
	Descriptors.bindingCount = ARRAYLEN(Bindings);
	Descriptors.pBindings = Bindings;

	Result = vkCreateDescriptorSetLayout(vkDevice, &Descriptors, NULL, &vkDescriptors);
	AssertEQ(Result, VK_SUCCESS);

	VkPipelineLayoutCreateInfo CreateInfo = {0};
	CreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	CreateInfo.pNext = NULL;
	CreateInfo.flags = 0;
	CreateInfo.setLayoutCount = 1;
	CreateInfo.pSetLayouts = &vkDescriptors;
	CreateInfo.pushConstantRangeCount = 0;
	CreateInfo.pPushConstantRanges = NULL;

	Result = vkCreatePipelineLayout(vkDevice, &CreateInfo, NULL, &vkPipelineLayout);
	AssertEQ(Result, VK_SUCCESS);


	VkGraphicsPipelineCreateInfo PipelineInfo = {0};
	PipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	PipelineInfo.pNext = NULL;
	PipelineInfo.flags = 0;
	PipelineInfo.stageCount = 2;
	PipelineInfo.pStages = Stages;
	PipelineInfo.pVertexInputState = &VertexInput;
	PipelineInfo.pInputAssemblyState = &InputAssembly;
	PipelineInfo.pTessellationState = NULL;
	PipelineInfo.pViewportState = &ViewportState;
	PipelineInfo.pRasterizationState = &Rasterizer;
	PipelineInfo.pMultisampleState = &Multisampling;
	PipelineInfo.pDepthStencilState = &DepthStencil;
	PipelineInfo.pColorBlendState = &Blending;
	PipelineInfo.pDynamicState = &DynamicState;
	PipelineInfo.layout = vkPipelineLayout;
	PipelineInfo.renderPass = vkRenderPass;
	PipelineInfo.subpass = 0;
	PipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	PipelineInfo.basePipelineIndex = -1;

	Result = vkCreateGraphicsPipelines(vkDevice, VK_NULL_HANDLE, 1, &PipelineInfo, NULL, &vkPipeline);
	AssertEQ(Result, VK_SUCCESS);

	VulkanDestroyShader(VertexModule);
	VulkanDestroyShader(FragmentModule);


	VulkanInitFramebuffers();


	VkDescriptorPoolSize PoolSizes[2] = {0};

	PoolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	PoolSizes[0].descriptorCount = vkImageCount;

	PoolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	PoolSizes[1].descriptorCount = vkImageCount;

	VkDescriptorPoolCreateInfo DescriptorInfo = {0};
	DescriptorInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	DescriptorInfo.pNext = NULL;
	DescriptorInfo.flags = 0;
	DescriptorInfo.maxSets = vkImageCount;
	DescriptorInfo.poolSizeCount = ARRAYLEN(PoolSizes);
	DescriptorInfo.pPoolSizes = PoolSizes;

	Result = vkCreateDescriptorPool(vkDevice, &DescriptorInfo, NULL, &vkDescriptorPool);
	AssertEQ(Result, VK_SUCCESS);
}


static void
VulkanDestroyPipeline(
	void
	)
{
	vkDestroyDescriptorPool(vkDevice, vkDescriptorPool, NULL);

	VulkanDestroyFramebuffers();

	vkDestroyPipeline(vkDevice, vkPipeline, NULL);
	vkDestroyPipelineLayout(vkDevice, vkPipelineLayout, NULL);
	vkDestroyDescriptorSetLayout(vkDevice, vkDescriptors, NULL);
	vkDestroyRenderPass(vkDevice, vkRenderPass, NULL);

	VulkanDestroyTexture(&vkTexture);
}


static void
VulkanInitObjects(
	void
	)
{
	VkFrame* Frame = vkFrames;

	do
	{
		VkSemaphoreCreateInfo SemaphoreInfo = {0};
		SemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		SemaphoreInfo.pNext = NULL;
		SemaphoreInfo.flags = 0;

		VkSemaphore* Semaphore = Frame->Semaphores;
		VkSemaphore* SemaphoreEnd = Frame->Semaphores + ARRAYLEN(Frame->Semaphores);

		do
		{
			VkResult Result = vkCreateSemaphore(vkDevice, &SemaphoreInfo, NULL, Semaphore);
			AssertEQ(Result, VK_SUCCESS);
		}
		while(++Semaphore != SemaphoreEnd);


		VkFenceCreateInfo FenceInfo = {0};
		FenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		FenceInfo.pNext = NULL;
		FenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		VkFence* Fence = Frame->Fences;
		VkFence* FenceEnd = Frame->Fences + ARRAYLEN(Frame->Fences);

		do
		{
			VkResult Result = vkCreateFence(vkDevice, &FenceInfo, NULL, Fence);
			AssertEQ(Result, VK_SUCCESS);
		}
		while(++Fence != FenceEnd);


		VulkanGetBuffer(sizeof(VkVertexUniformInput), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&Frame->UniformBuffer, &Frame->UniformBufferMemory);

		VkResult Result = vkMapMemory(vkDevice, Frame->UniformBufferMemory, 0, VK_WHOLE_SIZE, 0, &Frame->UniformData);
		AssertEQ(Result, VK_SUCCESS);


		VkDescriptorSetAllocateInfo AllocInfo = {0};
		AllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		AllocInfo.pNext = NULL;
		AllocInfo.descriptorPool = vkDescriptorPool;
		AllocInfo.descriptorSetCount = 1;
		AllocInfo.pSetLayouts = &vkDescriptors;

		Result = vkAllocateDescriptorSets(vkDevice, &AllocInfo, &Frame->DescriptorSet);
		AssertEQ(Result, VK_SUCCESS);


		VkDescriptorBufferInfo BufferInfo = {0};
		BufferInfo.buffer = Frame->UniformBuffer;
		BufferInfo.offset = 0;
		BufferInfo.range = VK_WHOLE_SIZE;

		VkDescriptorImageInfo ImageInfo = {0};
		ImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		ImageInfo.imageView = vkTexture.View;
		ImageInfo.sampler = vkSampler;

		VkWriteDescriptorSet DescriptorWrites[2] = {0};

		DescriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		DescriptorWrites[0].pNext = NULL;
		DescriptorWrites[0].dstSet = Frame->DescriptorSet;
		DescriptorWrites[0].dstBinding = 0;
		DescriptorWrites[0].dstArrayElement = 0;
		DescriptorWrites[0].descriptorCount = 1;
		DescriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		DescriptorWrites[0].pImageInfo = NULL;
		DescriptorWrites[0].pBufferInfo = &BufferInfo;
		DescriptorWrites[0].pTexelBufferView = NULL;

		DescriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		DescriptorWrites[1].pNext = NULL;
		DescriptorWrites[1].dstSet = Frame->DescriptorSet;
		DescriptorWrites[1].dstBinding = 1;
		DescriptorWrites[1].dstArrayElement = 0;
		DescriptorWrites[1].descriptorCount = 1;
		DescriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		DescriptorWrites[1].pImageInfo = &ImageInfo;
		DescriptorWrites[1].pBufferInfo = NULL;
		DescriptorWrites[1].pTexelBufferView = NULL;

		vkUpdateDescriptorSets(vkDevice, ARRAYLEN(DescriptorWrites), DescriptorWrites, 0, NULL);
	}
	while(++Frame != vkFrameEnd);
}


static void
VulkanDestroyObjects(
	void
	)
{
	VkFrame* Frame = vkFrames;

	do
	{
		vkFreeMemory(vkDevice, Frame->UniformBufferMemory, NULL);
		vkDestroyBuffer(vkDevice, Frame->UniformBuffer, NULL);

		VkFence* Fence = Frame->Fences;
		VkFence* FenceEnd = Frame->Fences + ARRAYLEN(Frame->Fences);

		do
		{
			vkDestroyFence(vkDevice, *Fence, NULL);
		}
		while(++Fence != FenceEnd);


		VkSemaphore* Semaphore = Frame->Semaphores;
		VkSemaphore* SemaphoreEnd = Frame->Semaphores + ARRAYLEN(Frame->Semaphores);

		do
		{
			vkDestroySemaphore(vkDevice, *Semaphore, NULL);
		}
		while(++Semaphore != SemaphoreEnd);
	}
	while(++Frame != vkFrameEnd);
}


static void
VulkanInitVertex(
	void
	)
{
	VulkanGetFinalBuffer(sizeof(vkVertexInput), &vkVertexBuffer, &vkVertexBufferMemory);

	VulkanCopyToBuffer(vkVertexBuffer, vkVertexInput, sizeof(vkVertexInput));
}


static void
VulkanDestroyVertex(
	void
	)
{
	vkFreeMemory(vkDevice, vkVertexBufferMemory, NULL);
	vkDestroyBuffer(vkDevice, vkVertexBuffer, NULL);
}


static void
VulkanRecordCommands(
	uint32_t ImageIndex
	)
{
	VkCommandBufferBeginInfo BeginInfo = {0};
	BeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	BeginInfo.pNext = NULL;
	BeginInfo.flags = 0;
	BeginInfo.pInheritanceInfo = NULL;

	VkResult Result = vkBeginCommandBuffer(vkFrame->CommandBuffer, &BeginInfo);
	AssertEQ(Result, VK_SUCCESS);

	VkDeviceSize Offset = 0;

	VkClearValue ClearValues[2] = {0};

	ClearValues[0].color = (VkClearColorValue){{ 0.0f, 0.0f, 0.0f, 0.0f }};
	ClearValues[1].depthStencil = (VkClearDepthStencilValue){ 1.0f, 0 };


	VkRenderPassBeginInfo RenderPassInfo = {0};
	RenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	RenderPassInfo.pNext = NULL;
	RenderPassInfo.renderPass = vkRenderPass;
	RenderPassInfo.framebuffer = vkFramebuffers[ImageIndex];
	RenderPassInfo.renderArea.offset.x = 0;
	RenderPassInfo.renderArea.offset.y = 0;
	RenderPassInfo.renderArea.extent = vkExtent;
	RenderPassInfo.clearValueCount = ARRAYLEN(ClearValues);
	RenderPassInfo.pClearValues = ClearValues;

	vkCmdBeginRenderPass(vkFrame->CommandBuffer, &RenderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdBindPipeline(vkFrame->CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vkPipeline);

	vkCmdSetViewport(vkFrame->CommandBuffer, 0, 1, &vkViewport);
	vkCmdSetScissor(vkFrame->CommandBuffer, 0, 1, &vkScissor);

	vkCmdBindVertexBuffers(vkFrame->CommandBuffer, 0, 1, &vkVertexBuffer, &Offset);
	vkCmdBindDescriptorSets(vkFrame->CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
		vkPipelineLayout, 0, 1, &vkFrame->DescriptorSet, 0, NULL);

	vkCmdDraw(vkFrame->CommandBuffer, ARRAYLEN(vkVertexInput), 1, 0, 0);

	vkCmdEndRenderPass(vkFrame->CommandBuffer);

	Result = vkEndCommandBuffer(vkFrame->CommandBuffer);
	AssertEQ(Result, VK_SUCCESS);
}


static void
VulkanRecreateSwapchain(
	void
	)
{
	vkExtent = VulkanGetExtent();
	VulkanGetConsts();

	vkDeviceWaitIdle(vkDevice);

	VulkanDestroyFramebuffers();
	VulkanDestroySwapchain();

	VulkanInitSwapchain();
	VulkanInitFramebuffers();
}


static void
VulkanUpdateUniforms(
	void
	)
{
	static float Rotation = 0;
	Rotation += 0.01;

	vec3 RotationAxis = { sinf(Rotation), sinf(Rotation) * cosf(Rotation), cosf(Rotation) };
	vec3 Eye = { 0.0f, 1.5f, 1.5f };
	vec3 Center = { 0.0f, 0.0f, 0.0f };
	vec3 Up = { 0.0f, 0.0f, 1.0f };


	VkVertexUniformInput Input = {0};
	glm_mat4_identity(Input.Model);
	glm_mat4_identity(Input.View);
	glm_mat4_identity(Input.Projection);

	glm_rotate(Input.Model, Rotation, RotationAxis);
	glm_lookat(Eye, Center, Up, Input.View);
	glm_perspective(glm_rad(45 + sinf(Rotation) * 40), (float) vkExtent.width / vkExtent.height, 0.1f, 100.0f, Input.Projection);
	Input.Projection[1][1] *= -1;

	memcpy(vkFrame->UniformData, &Input, sizeof(Input));
}


static void
VulkanDraw(
	void
	)
{
	VkResult Result = vkWaitForFences(vkDevice, 1, vkFrame->Fences + FENCE_IN_FLIGHT, VK_TRUE, UINT64_MAX);
	AssertEQ(Result, VK_SUCCESS);

	uint32_t ImageIndex;
	Result = vkAcquireNextImageKHR(vkDevice, vkSwapchain, UINT64_MAX,
		vkFrame->Semaphores[SEMAPHORE_IMAGE_AVAILABLE], VK_NULL_HANDLE, &ImageIndex);

	if(Result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		VulkanRecreateSwapchain();
		return;
	}

	Result = vkResetFences(vkDevice, 1, vkFrame->Fences + FENCE_IN_FLIGHT);
	AssertEQ(Result, VK_SUCCESS);


	Result = vkResetCommandBuffer(vkFrame->CommandBuffer, 0);
	AssertEQ(Result, VK_SUCCESS);

	VulkanRecordCommands(ImageIndex);

	VulkanUpdateUniforms();

	VkPipelineStageFlags WaitStages[] =
	{
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
	};

	VkSubmitInfo SubmitInfo = {0};
	SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	SubmitInfo.pNext = NULL;
	SubmitInfo.waitSemaphoreCount = 1;
	SubmitInfo.pWaitSemaphores = vkFrame->Semaphores + SEMAPHORE_IMAGE_AVAILABLE;
	SubmitInfo.pWaitDstStageMask = WaitStages;
	SubmitInfo.commandBufferCount = 1;
	SubmitInfo.pCommandBuffers = &vkFrame->CommandBuffer;
	SubmitInfo.signalSemaphoreCount = 1;
	SubmitInfo.pSignalSemaphores = vkFrame->Semaphores + SEMAPHORE_RENDER_FINISHED;

	Result = vkQueueSubmit(vkQueue, 1, &SubmitInfo, vkFrame->Fences[FENCE_IN_FLIGHT]);
	AssertEQ(Result, VK_SUCCESS);

	VkPresentInfoKHR PresentInfo = {0};
	PresentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	PresentInfo.pNext = NULL;
	PresentInfo.waitSemaphoreCount = 1;
	PresentInfo.pWaitSemaphores = vkFrame->Semaphores + SEMAPHORE_RENDER_FINISHED;
	PresentInfo.swapchainCount = 1;
	PresentInfo.pSwapchains = &vkSwapchain;
	PresentInfo.pImageIndices = &ImageIndex;
	PresentInfo.pResults = NULL;

	Result = vkQueuePresentKHR(vkQueue, &PresentInfo);

	if(Result == VK_ERROR_OUT_OF_DATE_KHR || Result == VK_SUBOPTIMAL_KHR)
	{
		VulkanRecreateSwapchain();
	}

	if(++vkFrame == vkFrameEnd)
	{
		vkFrame = vkFrames;
	}
}


void
VulkanInit(
	void
	)
{
	VulkanInitGLFW();
	VulkanInitInstance();
	VulkanInitSurface();
	VulkanInitDevice();
	VulkanInitSwapchain();
	VulkanInitSampler();
	VulkanInitDepthBuffer();
	VulkanInitFrames();
	VulkanInitCommands();
	VulkanInitPipeline();
	VulkanInitObjects();
	VulkanInitVertex();
}


void
VulkanRun(
	void
	)
{
	while(!glfwWindowShouldClose(Window))
	{
		glfwPollEvents();
		VulkanDraw();
	}

	vkDeviceWaitIdle(vkDevice);
}


void
VulkanFree(
	void
	)
{
	VulkanDestroyCopyBuffer();

	VulkanDestroyVertex();
	VulkanDestroyObjects();
	VulkanDestroyPipeline();
	VulkanDestroyCommands();
	VulkanDestroyFrames();
	VulkanDestroyDepthBuffer();
	VulkanDestroySampler();
	VulkanDestroySwapchain();
	VulkanDestroyDevice();
	VulkanDestroySurface();
	VulkanDestroyInstance();
	VulkanDestroyGLFW();
}
