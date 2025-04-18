#pragma once

#include <Common/Math/CoreNumericTypes.h>

#define DEFINE_SIMPLE_VULKAN_HANDLE(object) \
	struct object##_T; \
	using object = object##_T*;

#if PLATFORM_64BIT
#define DEFINE_VULKAN_HANDLE(object) DEFINE_SIMPLE_VULKAN_HANDLE(object)
#else
#define DEFINE_VULKAN_HANDLE(object) using object = ngine::uint64;
#endif

#if RENDERER_VULKAN
DEFINE_SIMPLE_VULKAN_HANDLE(VkPhysicalDevice)
DEFINE_SIMPLE_VULKAN_HANDLE(VkDevice)
DEFINE_VULKAN_HANDLE(VkBuffer)
DEFINE_VULKAN_HANDLE(VkDeviceMemory)
DEFINE_VULKAN_HANDLE(VkShaderModule)
DEFINE_VULKAN_HANDLE(VkCommandPool)
DEFINE_SIMPLE_VULKAN_HANDLE(VkCommandBuffer)
DEFINE_VULKAN_HANDLE(VkImage)
DEFINE_VULKAN_HANDLE(VkDescriptorPool)
DEFINE_SIMPLE_VULKAN_HANDLE(VkQueue)
DEFINE_SIMPLE_VULKAN_HANDLE(VkInstance)
DEFINE_VULKAN_HANDLE(VkDebugUtilsMessengerEXT)
DEFINE_VULKAN_HANDLE(VkRenderPass)
DEFINE_VULKAN_HANDLE(VkDescriptorSetLayout)
DEFINE_VULKAN_HANDLE(VkPipeline)
DEFINE_VULKAN_HANDLE(VkSampler)
DEFINE_VULKAN_HANDLE(VkSamplerYcbcrConversion);
DEFINE_VULKAN_HANDLE(VkDescriptorSet)
DEFINE_VULKAN_HANDLE(VkImageView)
DEFINE_VULKAN_HANDLE(VkFramebuffer)
DEFINE_VULKAN_HANDLE(VkFence)
DEFINE_VULKAN_HANDLE(VkSemaphore)
DEFINE_VULKAN_HANDLE(VkPipelineCache)
DEFINE_VULKAN_HANDLE(VkPipelineLayout)
DEFINE_VULKAN_HANDLE(VkSwapchainKHR)
DEFINE_VULKAN_HANDLE(VkSurfaceKHR)
DEFINE_VULKAN_HANDLE(VkAccelerationStructureKHR)

struct VkBufferCopy;
struct VkBufferImageCopy;
struct VkImageCopy;
struct VkImageBlit;
struct VkImageMemoryBarrier;
struct VkBufferMemoryBarrier;
union VkClearValue;
struct VkAccelerationStructureGeometryKHR;

#undef DEFINE_VULKAN_HANDLE
#endif
