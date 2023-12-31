#include "../include/vulkan.h"
#include "../include/debug.h"
#include "../include/util.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define CGLM_FORCE_RADIANS
#define CGLM_FORCE_LEFT_HANDED
#define CGLM_FORCE_DEPTH_ZERO_TO_ONE
#define CGLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <cglm/cglm.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include <time.h>
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
static VkSampleCountFlagBits vkSamples;
static VkPhysicalDeviceLimits vkLimits;
static uint32_t vkMinImageCount;
static VkSurfaceTransformFlagBitsKHR vkTransform;
static VkPhysicalDeviceMemoryProperties vkMemoryProperties;


static VkSwapchainKHR vkSwapchain;
static VkImage* vkImages;
static VkImageView* vkImageViews;
static uint32_t vkImageCount;


static VkCommandPool vkCommandPool;
static VkCommandBuffer vkCommandBuffer;
static VkFence vkFence;
static VkBuffer vkCopyBuffer;
static VkDeviceMemory vkCopyBufferMemory;


static VkSampler vkSampler;

typedef struct Image
{
	uint32_t Layers;
	VkImage Image;
	VkImageView View;
	VkDeviceMemory Memory;
}
Image;

static Image vkTexture;
static Image vkDepthBuffer;
static Image vkMultisampling;


typedef struct VkVertexVertexInput
{
	vec2 Position;
	vec2 TexCoord;
}
VkVertexVertexInput;

static const VkVertexVertexInput vkVertexVertexInput[] =
{
	{ { -0.5f, -0.5f }, { 0.0f, 0.0f } },
	{ {  0.5f, -0.5f }, { 1.0f, 0.0f } },
	{ { -0.5f,  0.5f }, { 0.0f, 1.0f } },
	{ {  0.5f,  0.5f }, { 1.0f, 1.0f } },
};

static VkBuffer vkVertexVertexInputBuffer;
static VkDeviceMemory vkVertexVertexInputMemory;


typedef struct VkVertexInstanceInput
{
	vec3 Position;
	vec2 Dimensions;
	float Rotation;
	uint32_t TexIndex;
}
VkVertexInstanceInput;

static const VkVertexInstanceInput vkVertexInstanceInput[] =
{
	{ { 0.0f, 0.0f, -50.0f }, { 50.0f, 50.0f }, 0, 0 },
	{ { 0.0f, 0.0f, -1.0f }, { 1.0f, 1.0f }, 0, 1 },
	{ { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f }, 1, 2 },
	{ { 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f }, 2, 3 },
};

static VkBuffer vkVertexInstanceInputBuffer;
static VkDeviceMemory vkVertexInstanceInputMemory;


typedef struct VkVertexConstantInput
{
	mat4 Transform;
}
VkVertexConstantInput;


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
	AppInfo.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo CreateInfo = {0};
	CreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	CreateInfo.flags = 0;
	CreateInfo.pApplicationInfo = &AppInfo;

	uint32_t GLFWExtensionCount;
	const char** GLFWExtensions = glfwGetRequiredInstanceExtensions(&GLFWExtensionCount);

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
#ifndef NDEBUG
	VulkanDestroyDebugUtilsMessengerEXT();
#endif
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
	VkSampleCountFlagBits Samples;
	VkSurfaceTransformFlagBitsKHR Transform;
	VkPhysicalDeviceLimits Limits;
}
VkDeviceScore;


static int
VulkanGetDeviceFeatures(
	VkPhysicalDevice Device,
	VkDeviceScore* DeviceScore
	)
{
	VkPhysicalDeviceFeatures Features;
	vkGetPhysicalDeviceFeatures(Device, &Features);

	if(!Features.samplerAnisotropy)
	{
		return 0;
	}

	if(!Features.sampleRateShading)
	{
		return 0;
	}

	return 1;
}


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


static int
VulkanGetDeviceProperties(
	VkPhysicalDevice Device,
	VkDeviceScore* DeviceScore
	)
{
	VkPhysicalDeviceProperties Properties;
	vkGetPhysicalDeviceProperties(Device, &Properties);

	if(Properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
	{
		DeviceScore->Score += 1000;
	}

	VkSampleCountFlags Counts =
		Properties.limits.framebufferColorSampleCounts & Properties.limits.framebufferDepthSampleCounts;

	if(Counts & VK_SAMPLE_COUNT_16_BIT)
	{
		DeviceScore->Samples = VK_SAMPLE_COUNT_16_BIT;
	}
	else if(Counts & VK_SAMPLE_COUNT_8_BIT)
	{
		DeviceScore->Samples = VK_SAMPLE_COUNT_8_BIT;
	}
	else if(Counts & VK_SAMPLE_COUNT_4_BIT)
	{
		DeviceScore->Samples = VK_SAMPLE_COUNT_4_BIT;
	}
	else if(Counts & VK_SAMPLE_COUNT_2_BIT)
	{
		DeviceScore->Samples = VK_SAMPLE_COUNT_2_BIT;
	}
	else
	{
		AssertEQ(0, 1);
	}

	DeviceScore->Score += DeviceScore->Samples * 16;
	DeviceScore->Score += Properties.limits.maxImageDimension2D;
	DeviceScore->Limits = Properties.limits;

	return 1;
}


static VkDeviceScore
VulkanGetDeviceScore(
	VkPhysicalDevice Device
	)
{
	VkDeviceScore DeviceScore = {0};

	if(!VulkanGetDeviceFeatures(Device, &DeviceScore))
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

	if(!VulkanGetDeviceProperties(Device, &DeviceScore))
	{
		goto goto_err;
	}

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

	VkPhysicalDevice BestDevice = NULL;
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

	AssertNEQ(BestDevice, NULL);

	vkQueueID = BestDeviceScore.QueueID;
	vkExtent = BestDeviceScore.Extent;
	vkSamples = BestDeviceScore.Samples;
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
	DeviceFeatures.samplerAnisotropy = VK_TRUE;
	DeviceFeatures.sampleRateShading = VK_TRUE;

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
	CreateInfo.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
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
		VkImageViewCreateInfo CreateInfo = {0};
		CreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		CreateInfo.pNext = NULL;
		CreateInfo.flags = 0;
		CreateInfo.image = *Image;
		CreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		CreateInfo.format = VK_FORMAT_B8G8R8A8_SRGB;
		CreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		CreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		CreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		CreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		CreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		CreateInfo.subresourceRange.baseMipLevel = 0;
		CreateInfo.subresourceRange.levelCount = 1;
		CreateInfo.subresourceRange.baseArrayLayer = 0;
		CreateInfo.subresourceRange.layerCount = 1;

		VkResult Result = vkCreateImageView(vkDevice, &CreateInfo, NULL, ImageView);
		AssertEQ(Result, VK_SUCCESS);

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
	CreateInfo.magFilter = VK_FILTER_NEAREST;
	CreateInfo.minFilter = VK_FILTER_NEAREST;
	CreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	CreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	CreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	CreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
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

	memcpy(Memory, Data, Size);
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
	uint32_t TextureWidth,
	uint32_t TextureHeight,
	uint32_t TextureColumns,
	uint32_t TextureRows
	)
{
	VulkanBeginCommandBuffer();

	VulkanDestroyCopyBuffer();

	VkDeviceSize Pass = TextureWidth * 4;
	VkDeviceSize BigPass = Pass * 7;
	VkDeviceSize Size = Pass * TextureHeight * TextureColumns * TextureRows;

	VulkanGetStagingBuffer(Size, &vkCopyBuffer, &vkCopyBufferMemory);

	void* Memory;

	VkResult Result = vkMapMemory(vkDevice, vkCopyBufferMemory, 0, VK_WHOLE_SIZE, 0, &Memory);
	AssertEQ(Result, VK_SUCCESS);

	memcpy(Memory, Data, Size);
	vkUnmapMemory(vkDevice, vkCopyBufferMemory);

	uint32_t ImageWidth = TextureWidth * TextureColumns;
	uint32_t ImageHeight = TextureHeight * TextureRows;

	VkBufferImageCopy Copies[Image->Layers];

	uint32_t i = 0;
	VkBufferImageCopy* Copy = Copies;
	VkDeviceSize BufferOffset = 0;

	while(1)
	{
		*Copy = (VkBufferImageCopy){0};
		Copy->bufferOffset = BufferOffset;
		Copy->bufferRowLength = ImageWidth;
		Copy->bufferImageHeight = ImageHeight;
		Copy->imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		Copy->imageSubresource.mipLevel = 0;
		Copy->imageSubresource.baseArrayLayer = i;
		Copy->imageSubresource.layerCount = 1;
		Copy->imageOffset.x = 0;
		Copy->imageOffset.y = 0;
		Copy->imageOffset.z = 0;
		Copy->imageExtent.width = TextureWidth;
		Copy->imageExtent.height = TextureHeight;
		Copy->imageExtent.depth = 1;

		if(++i == Image->Layers)
		{
			break;
		}

		if(i % TextureColumns == 0)
		{
			BufferOffset += BigPass;
		}
		else
		{
			BufferOffset += Pass;
		}

		++Copy;
	}

	vkCmdCopyBufferToImage(vkCommandBuffer, vkCopyBuffer, Image->Image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, ARRAYLEN(Copies), Copies);

	VulkanEndCommandBuffer();
}


static void
VulkanCreateImageGeneric(
	uint32_t Width,
	uint32_t Height,
	VkFormat Format,
	uint32_t Layers,
	VkImageAspectFlags Aspect,
	VkSampleCountFlagBits Samples,
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
	ImageInfo.arrayLayers = Layers;
	ImageInfo.samples = Samples;
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

	VkImageViewCreateInfo ViewInfo = {0};
	ViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	ViewInfo.pNext = NULL;
	ViewInfo.flags = 0;
	ViewInfo.image = Image->Image;
	ViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
	ViewInfo.format = Format;
	ViewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	ViewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	ViewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	ViewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	ViewInfo.subresourceRange.aspectMask = Aspect;
	ViewInfo.subresourceRange.baseMipLevel = 0;
	ViewInfo.subresourceRange.levelCount = 1;
	ViewInfo.subresourceRange.baseArrayLayer = 0;
	ViewInfo.subresourceRange.layerCount = Layers;

	Result = vkCreateImageView(vkDevice, &ViewInfo, NULL, &Image->View);
	AssertEQ(Result, VK_SUCCESS);

	Image->Layers = Layers;
}


static void
VulkanCreateTextureImage(
	uint32_t TextureWidth,
	uint32_t TextureHeight,
	uint32_t Layers,
	Image* Image
	)
{
	VulkanCreateImageGeneric(TextureHeight, TextureHeight, VK_FORMAT_B8G8R8A8_SRGB, Layers,
		VK_IMAGE_ASPECT_COLOR_BIT, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_TRANSFER_DST_BIT |
		VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, Image);
}


static void
VulkanCreateDepthImage(
	Image* Image
	)
{
	VulkanCreateImageGeneric(vkExtent.width, vkExtent.height, VK_FORMAT_D32_SFLOAT, 1,
		VK_IMAGE_ASPECT_DEPTH_BIT, vkSamples, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, Image);
}


static void
VulkanCreateMultisampledImage(
	Image* Image
	)
{
	VulkanCreateImageGeneric(vkExtent.width, vkExtent.height, VK_FORMAT_B8G8R8A8_SRGB, 1,
		VK_IMAGE_ASPECT_COLOR_BIT, vkSamples, VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, Image);
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
	Barrier.subresourceRange.layerCount = Image->Layers;

	vkCmdPipelineBarrier(vkCommandBuffer, SourceStage, DestinationStage, 0, 0, NULL, 0, NULL, 1, &Barrier);

	VulkanEndCommandBuffer();
}


static void
VulkanCreateTexture(
	const char* Path,
	Image* Image
	)
{
	int ImageWidth;
	int ImageHeight;
	int ImageChannels;

	stbi_uc* Pixels = stbi_load(Path, &ImageWidth, &ImageHeight, &ImageChannels, STBI_rgb_alpha);
	AssertNEQ(Pixels, NULL);

	uint32_t TextureWidth;
	uint32_t TextureHeight;
	uint32_t TextureLayers;

	const char* File = Path + strlen(Path);

	while(File != Path && *(File - 1) != '/')
	{
		--File;
	}

	int Parsed = sscanf(File, "%ux%ux%u", &TextureWidth, &TextureHeight, &TextureLayers);
	AssertEQ(Parsed, 3);
	AssertEQ(ImageWidth % TextureWidth, 0);
	AssertEQ(ImageHeight % TextureHeight, 0);

	VulkanCreateTextureImage(TextureWidth, TextureHeight, TextureLayers, Image);

	VulkanTransitionImageLayout(Image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	VulkanCopyToImage(Image, Pixels, TextureWidth, TextureHeight,
		ImageWidth / TextureWidth, ImageHeight / TextureHeight);

	stbi_image_free(Pixels);

	VulkanTransitionImageLayout(Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
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
	VulkanCreateDepthImage(&vkDepthBuffer);
}


static void
VulkanDestroyDepthBuffer(
	void
	)
{
	VulkanDestroyImage(&vkDepthBuffer);
}


static void
VulkanInitMultisampling(
	void
	)
{
	VulkanCreateMultisampledImage(&vkMultisampling);
}


static void
VulkanDestroyMultisampling(
	void
	)
{
	VulkanDestroyImage(&vkMultisampling);
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
			vkMultisampling.View,
			vkDepthBuffer.View,
			*ImageView
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
VulkanInitPipeline(
	void
	)
{
	VulkanCreateTexture("textures/4x4x4.png", &vkTexture);

	VkAttachmentReference ColorAttachmentRef = {0};
	ColorAttachmentRef.attachment = 0;
	ColorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference DepthAttachmentRef = {0};
	DepthAttachmentRef.attachment = 1;
	DepthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference MultisamplingRef = {0};
	MultisamplingRef.attachment = 2;
	MultisamplingRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription Subpass = {0};
	Subpass.flags = 0;
	Subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	Subpass.inputAttachmentCount = 0;
	Subpass.pInputAttachments = NULL;
	Subpass.colorAttachmentCount = 1;
	Subpass.pColorAttachments = &ColorAttachmentRef;
	Subpass.pResolveAttachments = &MultisamplingRef;
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

	VkAttachmentDescription Attachments[3] = {0};

	Attachments[0].flags = 0;
	Attachments[0].format = VK_FORMAT_B8G8R8A8_SRGB;
	Attachments[0].samples = vkSamples;
	Attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	Attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	Attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	Attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	Attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	Attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	Attachments[1].flags = 0;
	Attachments[1].format = VK_FORMAT_D32_SFLOAT;
	Attachments[1].samples = vkSamples;
	Attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	Attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	Attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	Attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	Attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	Attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	Attachments[2].flags = 0;
	Attachments[2].format = VK_FORMAT_B8G8R8A8_SRGB;
	Attachments[2].samples = VK_SAMPLE_COUNT_1_BIT;
	Attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	Attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	Attachments[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	Attachments[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	Attachments[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	Attachments[2].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

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

	VkVertexInputBindingDescription VertexBindings[2] = {0};

	VertexBindings[0].binding = 0;
	VertexBindings[0].stride = sizeof(VkVertexVertexInput);
	VertexBindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VertexBindings[1].binding = 1;
	VertexBindings[1].stride = sizeof(VkVertexInstanceInput);
	VertexBindings[1].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

	VkVertexInputAttributeDescription Attributes[6] = {0};

	Attributes[0].location = 0;
	Attributes[0].binding = 0;
	Attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	Attributes[0].offset = offsetof(VkVertexVertexInput, Position);

	Attributes[1].location = 1;
	Attributes[1].binding = 0;
	Attributes[1].format = VK_FORMAT_R32G32_SFLOAT;
	Attributes[1].offset = offsetof(VkVertexVertexInput, TexCoord);

	Attributes[2].location = 2;
	Attributes[2].binding = 1;
	Attributes[2].format = VK_FORMAT_R32G32B32_SFLOAT;
	Attributes[2].offset = offsetof(VkVertexInstanceInput, Position);

	Attributes[3].location = 3;
	Attributes[3].binding = 1;
	Attributes[3].format = VK_FORMAT_R32G32_SFLOAT;
	Attributes[3].offset = offsetof(VkVertexInstanceInput, Dimensions);

	Attributes[4].location = 4;
	Attributes[4].binding = 1;
	Attributes[4].format = VK_FORMAT_R32_SFLOAT;
	Attributes[4].offset = offsetof(VkVertexInstanceInput, Rotation);

	Attributes[5].location = 5;
	Attributes[5].binding = 1;
	Attributes[5].format = VK_FORMAT_R32_UINT;
	Attributes[5].offset = offsetof(VkVertexInstanceInput, TexIndex);

	VkPipelineVertexInputStateCreateInfo VertexInput = {0};
	VertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	VertexInput.pNext = NULL;
	VertexInput.flags = 0;
	VertexInput.vertexBindingDescriptionCount = ARRAYLEN(VertexBindings);
	VertexInput.pVertexBindingDescriptions = VertexBindings;
	VertexInput.vertexAttributeDescriptionCount = ARRAYLEN(Attributes);
	VertexInput.pVertexAttributeDescriptions = Attributes;

	VkPipelineInputAssemblyStateCreateInfo InputAssembly = {0};
	InputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	InputAssembly.pNext = NULL;
	InputAssembly.flags = 0;
	InputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	InputAssembly.primitiveRestartEnable = VK_FALSE;

	VkViewport Viewport = {0};
	Viewport.x = 0.0f;
	Viewport.y = 0.0f;
	Viewport.width = vkExtent.width;
	Viewport.height = vkExtent.height;
	Viewport.minDepth = 0.0f;
	Viewport.maxDepth = 1.0f;

	VkRect2D Scissor = {0};
	Scissor.offset.x = 0;
	Scissor.offset.y = 0;
	Scissor.extent = vkExtent;

	VkPipelineViewportStateCreateInfo ViewportState = {0};
	ViewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	ViewportState.pNext = NULL;
	ViewportState.flags = 0;
	ViewportState.viewportCount = 1;
	ViewportState.pViewports = &Viewport;
	ViewportState.scissorCount = 1;
	ViewportState.pScissors = &Scissor;

	VkPipelineRasterizationStateCreateInfo Rasterizer = {0};
	Rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	Rasterizer.pNext = NULL;
	Rasterizer.flags = 0;
	Rasterizer.depthClampEnable = VK_FALSE;
	Rasterizer.rasterizerDiscardEnable = VK_FALSE;
	Rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	Rasterizer.cullMode = VK_CULL_MODE_NONE; // VK_CULL_MODE_BACK_BIT;
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
	Multisampling.rasterizationSamples = vkSamples;
	Multisampling.sampleShadingEnable = VK_TRUE;
	Multisampling.minSampleShading = 1.0f;
	Multisampling.pSampleMask = NULL;
	Multisampling.alphaToCoverageEnable = VK_TRUE;
	Multisampling.alphaToOneEnable = VK_FALSE;

	VkPipelineDepthStencilStateCreateInfo DepthStencil = {0};
	DepthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	DepthStencil.pNext = NULL;
	DepthStencil.flags = 0;
	DepthStencil.depthTestEnable = VK_TRUE;
	DepthStencil.depthWriteEnable = VK_TRUE;
	DepthStencil.depthCompareOp = VK_COMPARE_OP_GREATER;
	DepthStencil.depthBoundsTestEnable = VK_FALSE;
	DepthStencil.stencilTestEnable = VK_FALSE;
	DepthStencil.front = (VkStencilOpState){0};
	DepthStencil.back = (VkStencilOpState){0};
	DepthStencil.minDepthBounds = 0.0f;
	DepthStencil.maxDepthBounds = 0.0f;

	VkPipelineColorBlendAttachmentState BlendingAttachment = {0};
	BlendingAttachment.blendEnable = VK_TRUE;
	BlendingAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	BlendingAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	BlendingAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	BlendingAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	BlendingAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	BlendingAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
	BlendingAttachment.colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	VkPipelineColorBlendStateCreateInfo Blending = {0};
	Blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	Blending.pNext = NULL;
	Blending.flags = 0;
	Blending.logicOpEnable = VK_FALSE;
	Blending.logicOp = VK_LOGIC_OP_COPY;
	Blending.attachmentCount = 1;
	Blending.pAttachments = &BlendingAttachment;
	Blending.blendConstants[0] = 0.0f;
	Blending.blendConstants[1] = 0.0f;
	Blending.blendConstants[2] = 0.0f;
	Blending.blendConstants[3] = 0.0f;

	VkDescriptorSetLayoutBinding Bindings[1] = {0};

	Bindings[0].binding = 0;
	Bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	Bindings[0].descriptorCount = 1;
	Bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	Bindings[0].pImmutableSamplers = NULL;

	VkDescriptorSetLayoutCreateInfo Descriptors = {0};
	Descriptors.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	Descriptors.pNext = NULL;
	Descriptors.flags = 0;
	Descriptors.bindingCount = ARRAYLEN(Bindings);
	Descriptors.pBindings = Bindings;

	Result = vkCreateDescriptorSetLayout(vkDevice, &Descriptors, NULL, &vkDescriptors);
	AssertEQ(Result, VK_SUCCESS);

	VkPushConstantRange PushConstants[1] = {0};

	PushConstants[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	PushConstants[0].offset = 0;
	PushConstants[0].size = sizeof(VkVertexConstantInput);

	VkPipelineLayoutCreateInfo CreateInfo = {0};
	CreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	CreateInfo.pNext = NULL;
	CreateInfo.flags = 0;
	CreateInfo.setLayoutCount = 1;
	CreateInfo.pSetLayouts = &vkDescriptors;
	CreateInfo.pushConstantRangeCount = ARRAYLEN(PushConstants);
	CreateInfo.pPushConstantRanges = PushConstants;

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
	PipelineInfo.pDynamicState = NULL;
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


		VkDescriptorSetAllocateInfo AllocInfo = {0};
		AllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		AllocInfo.pNext = NULL;
		AllocInfo.descriptorPool = vkDescriptorPool;
		AllocInfo.descriptorSetCount = 1;
		AllocInfo.pSetLayouts = &vkDescriptors;

		VkResult Result = vkAllocateDescriptorSets(vkDevice, &AllocInfo, &Frame->DescriptorSet);
		AssertEQ(Result, VK_SUCCESS);


		VkDescriptorImageInfo ImageInfo = {0};
		ImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		ImageInfo.imageView = vkTexture.View;
		ImageInfo.sampler = vkSampler;

		VkWriteDescriptorSet DescriptorWrites[1] = {0};

		DescriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		DescriptorWrites[0].pNext = NULL;
		DescriptorWrites[0].dstSet = Frame->DescriptorSet;
		DescriptorWrites[0].dstBinding = 0;
		DescriptorWrites[0].dstArrayElement = 0;
		DescriptorWrites[0].descriptorCount = 1;
		DescriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		DescriptorWrites[0].pImageInfo = &ImageInfo;
		DescriptorWrites[0].pBufferInfo = NULL;
		DescriptorWrites[0].pTexelBufferView = NULL;

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
	VulkanGetFinalBuffer(sizeof(vkVertexVertexInput), &vkVertexVertexInputBuffer, &vkVertexVertexInputMemory);

	VulkanCopyToBuffer(vkVertexVertexInputBuffer, vkVertexVertexInput, sizeof(vkVertexVertexInput));


	VulkanGetFinalBuffer(sizeof(vkVertexInstanceInput), &vkVertexInstanceInputBuffer, &vkVertexInstanceInputMemory);

	VulkanCopyToBuffer(vkVertexInstanceInputBuffer, vkVertexInstanceInput, sizeof(vkVertexInstanceInput));
}


static void
VulkanDestroyVertex(
	void
	)
{
	vkFreeMemory(vkDevice, vkVertexInstanceInputMemory, NULL);
	vkDestroyBuffer(vkDevice, vkVertexInstanceInputBuffer, NULL);

	vkFreeMemory(vkDevice, vkVertexVertexInputMemory, NULL);
	vkDestroyBuffer(vkDevice, vkVertexVertexInputBuffer, NULL);
}


static void
VulkanUpdateConstants(
	void
	)
{
	static float Rotation = 0;
	Rotation += 0.0001;

	float AspectRatio = (float) vkExtent.width / vkExtent.height;
	vec3 RotationAxis = { 0.0f, 1.0f, 0.0f };
	vec3 Eye = { 0.0f, 0.001f, 3.0f };
	vec3 Center = { 0.0f, 0.0f, 0.0f };
	vec3 Up = { 0.0f, 0.0f, 1.0f };

	mat4 Model;
	glm_mat4_identity(Model);
	glm_rotate(Model, Rotation, RotationAxis);

	mat4 View;
	glm_mat4_identity(View);
	glm_lookat(Eye, Center, Up, View);

	mat4 Projection;
	glm_mat4_identity(Projection);
	glm_perspective(glm_rad(45), AspectRatio, 1000.0f, 0.01f, Projection);
	//glm_ortho_default((float) vkExtent.width / vkExtent.height, Input.Projection);

	mat4* Matrices[] =
	{
		&Projection,
		&View,
		&Model
	};

	VkVertexConstantInput Constants = {0};
	glm_mat4_mulN(Matrices, ARRAYLEN(Matrices), Constants.Transform);

	vkCmdPushConstants(vkFrame->CommandBuffer, vkPipelineLayout,
		VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Constants), &Constants);
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
	ClearValues[1].depthStencil = (VkClearDepthStencilValue){ 0.0f, 0 };


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

	vkCmdBindVertexBuffers(vkFrame->CommandBuffer, 0, 1, &vkVertexVertexInputBuffer, &Offset);
	vkCmdBindVertexBuffers(vkFrame->CommandBuffer, 1, 1, &vkVertexInstanceInputBuffer, &Offset);

	vkCmdBindDescriptorSets(vkFrame->CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
		vkPipelineLayout, 0, 1, &vkFrame->DescriptorSet, 0, NULL);

	VulkanUpdateConstants();

	vkCmdDraw(vkFrame->CommandBuffer, ARRAYLEN(vkVertexVertexInput), ARRAYLEN(vkVertexInstanceInput), 0, 0);

	vkCmdEndRenderPass(vkFrame->CommandBuffer);

	Result = vkEndCommandBuffer(vkFrame->CommandBuffer);
	AssertEQ(Result, VK_SUCCESS);
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
		vkExtent = VulkanGetExtent();

		return;
	}

	Result = vkResetFences(vkDevice, 1, vkFrame->Fences + FENCE_IN_FLIGHT);
	AssertEQ(Result, VK_SUCCESS);


	Result = vkResetCommandBuffer(vkFrame->CommandBuffer, 0);
	AssertEQ(Result, VK_SUCCESS);

	VulkanRecordCommands(ImageIndex);

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
		vkExtent = VulkanGetExtent();
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
	VulkanInitSampler();
	VulkanInitSwapchain();
	VulkanInitDepthBuffer();
	VulkanInitMultisampling();
	VulkanInitFrames();
	VulkanInitCommands();
	VulkanInitPipeline();
	VulkanInitObjects();
	VulkanInitVertex();
}

static long double fps[10000];
static uintptr_t fps_c = 0;

void
VulkanRun(
	void
	)
{
	while(!glfwWindowShouldClose(Window))
	{
		glfwPollEvents();

		struct timespec start = {0};
		clock_gettime(CLOCK_REALTIME, &start);
		uint64_t start_time = start.tv_nsec + start.tv_sec * 1000000000;

		VulkanDraw();

		struct timespec end = {0};
		clock_gettime(CLOCK_REALTIME, &end);
		uint64_t end_time = end.tv_nsec + end.tv_sec * 1000000000;

		fps[fps_c] = (long double) 1000000000.0 / (end_time - start_time);
		fps_c = (fps_c + 1) % ARRAYLEN(fps);

		if(fps_c == 0)
		{
			long double times = 0.0;

			for(uintptr_t i = 0; i < ARRAYLEN(fps); ++i)
			{
				times += fps[i];
			}

			times /= (long double) ARRAYLEN(fps);

			printf("avg fps %.02Lf\n", times);
		}
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
	VulkanDestroyMultisampling();
	VulkanDestroySwapchain();
	VulkanDestroySampler();
	VulkanDestroyDevice();
	VulkanDestroySurface();
	VulkanDestroyInstance();
	VulkanDestroyGLFW();
}

// 2. bring back swapchain lule cuz wthout it windows is broken af af af
