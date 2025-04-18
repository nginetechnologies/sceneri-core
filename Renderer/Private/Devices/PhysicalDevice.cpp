#include "Devices/PhysicalDevice.h"

#include <Renderer/InstanceView.h>
#include <Renderer/Vulkan/Includes.h>
#include <Renderer/WebGPU/Includes.h>
#include <Renderer/Metal/Includes.h>

#if PLATFORM_APPLE && RENDERER_VULKAN
#include <3rdparty/vulkan/vk_mvk_moltenvk.h>
#endif

#if RENDERER_WEBGPU
#include <Renderer/Window/Window.h>
#endif

#include <Common/Memory/Containers/Vector.h>
#include <Common/Memory/Containers/StringView.h>
#include <Common/Memory/Containers/RestrictedArrayView.h>

#include <cstring>

namespace ngine::Rendering
{
#if RENDERER_VULKAN
	inline bool CanSupportSwapchain(const PhysicalDeviceView device)
	{
		uint32 extensionCount;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

		FixedSizeVector<VkExtensionProperties, uint16>
			availableExtensions(Memory::ConstructWithSize, Memory::Uninitialized, (uint16)extensionCount);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.GetData());

		return availableExtensions.FindIf(
						 [](const VkExtensionProperties& property) -> bool
						 {
							 return !strcmp(VK_KHR_SWAPCHAIN_EXTENSION_NAME, property.extensionName);
						 }
					 ) != availableExtensions.end();
	}
#endif

	PhysicalDevice::PhysicalDevice(const PhysicalDeviceView device, [[maybe_unused]] const InstanceView instance, const unsigned int rating)
		: m_device(device)
		, m_rating(rating)
#if RENDERER_VULKAN
		, m_canSupportSwapchain(CanSupportSwapchain(device))
#elif RENDERER_METAL
#if PLATFORM_APPLE_VISIONOS || PLATFORM_APPLE_IOS
		, m_canSupportSwapchain(true)
#else
		, m_canSupportSwapchain(![(id<MTLDevice>)device isHeadless])
#endif
#elif RENDERER_WEBGPU
		, m_canSupportSwapchain(true)
#endif
	{
#if RENDERER_VULKAN
		{
			VkPhysicalDeviceFeatures2 supportedFeatures2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, nullptr};

			VkPhysicalDeviceVulkan12Features supportedDeviceVulkan1_2Features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES, nullptr};
			supportedFeatures2.pNext = &supportedDeviceVulkan1_2Features;

			VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipelineFeatures{
				VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
				supportedFeatures2.pNext
			};
			supportedFeatures2.pNext = &rayTracingPipelineFeatures;

			VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures{
				VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
				supportedFeatures2.pNext
			};
			supportedFeatures2.pNext = &accelerationStructureFeatures;

			VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures{
				VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR,
				supportedFeatures2.pNext
			};
			supportedFeatures2.pNext = &rayQueryFeatures;

			VkPhysicalDeviceExtendedDynamicStateFeaturesEXT extendedDynamicStateFeatures{
				VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT,
				supportedFeatures2.pNext
			};
			supportedFeatures2.pNext = &extendedDynamicStateFeatures;

			{
				PFN_vkGetPhysicalDeviceFeatures2 vkGetPhysicalDeviceFeatures2_func =
					reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2>((void*)vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceFeatures2"));
				if (vkGetPhysicalDeviceFeatures2_func == nullptr)
				{
					// In Vulkan 1.0 might be accessible under its original extension name
					vkGetPhysicalDeviceFeatures2_func =
						reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2>((void*)vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceFeatures2KHR"));
				}

				if (vkGetPhysicalDeviceFeatures2_func != nullptr)
				{
					vkGetPhysicalDeviceFeatures2_func(device, &supportedFeatures2);
				}
				else
				{
					vkGetPhysicalDeviceFeatures(device, &supportedFeatures2.features);
				}
			}

			m_supportedFeatures |= PhysicalDeviceFeatures::GeometryShader * (bool)supportedFeatures2.features.geometryShader;
			m_supportedFeatures |= PhysicalDeviceFeatures::TesselationShader * (bool)supportedFeatures2.features.tessellationShader;
			m_supportedFeatures |= PhysicalDeviceFeatures::TextureCompressionBC * (bool)supportedFeatures2.features.textureCompressionBC;
			m_supportedFeatures |= PhysicalDeviceFeatures::TextureCompressionETC2 * (bool)supportedFeatures2.features.textureCompressionETC2;
			m_supportedFeatures |= PhysicalDeviceFeatures::TextureCompressionASTC_LDR *
			                       (bool)supportedFeatures2.features.textureCompressionASTC_LDR;
			m_supportedFeatures |= PhysicalDeviceFeatures::DepthClamp * (bool)supportedFeatures2.features.depthClamp;
			m_supportedFeatures |= PhysicalDeviceFeatures::DepthBounds * (bool)supportedFeatures2.features.depthBounds;
			m_supportedFeatures |= PhysicalDeviceFeatures::DepthBiasClamp * (bool)supportedFeatures2.features.depthBiasClamp;
			m_supportedFeatures |= PhysicalDeviceFeatures::CubemapArrays * (bool)supportedFeatures2.features.imageCubeArray;
			m_supportedFeatures |= PhysicalDeviceFeatures::NonSolidFillMode * (bool)supportedFeatures2.features.fillModeNonSolid;
			m_supportedFeatures |= PhysicalDeviceFeatures::ShaderInt16 * (bool)supportedFeatures2.features.shaderInt16;
			m_supportedFeatures |= PhysicalDeviceFeatures::ShaderInt64 * (bool)supportedFeatures2.features.shaderInt64;
			m_supportedFeatures |= PhysicalDeviceFeatures::FragmentStoresAndAtomics * (bool)supportedFeatures2.features.fragmentStoresAndAtomics;
			m_supportedFeatures |= PhysicalDeviceFeatures::VertexPipelineStoresAndAtomics *
			                       (bool)supportedFeatures2.features.vertexPipelineStoresAndAtomics;

			m_supportedFeatures |= PhysicalDeviceFeatures::ShaderFloat16 * (bool)(supportedDeviceVulkan1_2Features.shaderFloat16);
			m_supportedFeatures |= PhysicalDeviceFeatures::DescriptorIndexing * (bool)(supportedDeviceVulkan1_2Features.descriptorIndexing);
			m_supportedFeatures |= PhysicalDeviceFeatures::BufferDeviceAddress * (bool)(supportedDeviceVulkan1_2Features.bufferDeviceAddress);
			m_supportedFeatures |= PhysicalDeviceFeatures::PartiallyBoundDescriptorBindings *
			                       (bool)(supportedDeviceVulkan1_2Features.descriptorBindingPartiallyBound);
			m_supportedFeatures |= PhysicalDeviceFeatures::RuntimeDescriptorArrays *
			                       (bool)(supportedDeviceVulkan1_2Features.runtimeDescriptorArray);
			m_supportedFeatures |= PhysicalDeviceFeatures::UpdateDescriptorSampleImageAfterBind *
			                       (bool)(supportedDeviceVulkan1_2Features.descriptorBindingSampledImageUpdateAfterBind);
			m_supportedFeatures |= PhysicalDeviceFeatures::NonUniformImageArrayIndexing *
			                       (bool)(supportedDeviceVulkan1_2Features.shaderSampledImageArrayNonUniformIndexing);
			m_supportedFeatures |= PhysicalDeviceFeatures::SeparateDepthStencilLayout *
			                       (bool)(supportedDeviceVulkan1_2Features.separateDepthStencilLayouts);

#if ADVERTISE_RAYTRACING_SUPPORT
			m_supportedFeatures |= PhysicalDeviceFeatures::RayTracingPipeline * (bool)(rayTracingPipelineFeatures.rayTracingPipeline);

			m_supportedFeatures |= PhysicalDeviceFeatures::AccelerationStructure * (bool)(accelerationStructureFeatures.accelerationStructure);
			m_supportedFeatures |= PhysicalDeviceFeatures::AccelerationStructureHostCommands *
			                       (bool)(accelerationStructureFeatures.accelerationStructureHostCommands);

			m_supportedFeatures |= PhysicalDeviceFeatures::RayQuery * (bool)(rayQueryFeatures.rayQuery);
#endif

			m_supportedFeatures |= PhysicalDeviceFeatures::ExtendedDynamicState * (bool)(extendedDynamicStateFeatures.extendedDynamicState);

#if PLATFORM_APPLE
			MVKPhysicalDeviceMetalFeatures metalFeatures;
			size_t featuresSize = sizeof(metalFeatures);
			[[maybe_unused]] const VkResult result = vkGetPhysicalDeviceMetalFeaturesMVK(device, &metalFeatures, &featuresSize);
			Assert(result == VK_SUCCESS || result == VK_INCOMPLETE);

			id<MTLDevice> mtlDevice{nil};
			vkGetMTLDeviceMVK(device, &mtlDevice);

			m_supportedFeatures |= PhysicalDeviceFeatures::CubemapReadWrite * (bool)supportedFeatures2.features.imageCubeArray;
			m_supportedFeatures |= PhysicalDeviceFeatures::LayeredRendering * (bool)metalFeatures.layeredRendering;
			m_supportedFeatures |= PhysicalDeviceFeatures::ReadWriteBuffers *
			                       ([mtlDevice supportsFamily:MTLGPUFamilyCommon2] || [mtlDevice supportsFamily:MTLGPUFamilyApple3]);
			m_supportedFeatures |= PhysicalDeviceFeatures::ReadWriteTextures *
			                       ([mtlDevice supportsFamily:MTLGPUFamilyCommon3] || [mtlDevice supportsFamily:MTLGPUFamilyApple4]);
#else
			// Assume it's always supported
			m_supportedFeatures |= PhysicalDeviceFeatures::CubemapReadWrite;
			m_supportedFeatures |= PhysicalDeviceFeatures::LayeredRendering;
			m_supportedFeatures |= PhysicalDeviceFeatures::ReadWriteBuffers;
			m_supportedFeatures |= PhysicalDeviceFeatures::ReadWriteTextures;
#endif
		}

#elif RENDERER_METAL
		id<MTLDevice> mtlDevice = (id<MTLDevice>)device;

		if (@available(macCatalyst 16.4, iOS 16.4, *))
		{
			m_supportedFeatures |= PhysicalDeviceFeatures::TextureCompressionBC * [mtlDevice supportsBCTextureCompression];
		}
		else
		{
			m_supportedFeatures |= PhysicalDeviceFeatures::TextureCompressionBC;
		}

		m_supportedFeatures |= PhysicalDeviceFeatures::TextureCompressionETC2 * [mtlDevice supportsFamily:MTLGPUFamilyApple2];
		m_supportedFeatures |= PhysicalDeviceFeatures::TextureCompressionASTC_LDR * [mtlDevice supportsFamily:MTLGPUFamilyApple2];
#if PLATFORM_APPLE_IOS && !PLATFORM_APPLE_MACCATALYST
		m_supportedFeatures |= PhysicalDeviceFeatures::DepthClamp * [mtlDevice supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily2_v4];
#else
		m_supportedFeatures |= PhysicalDeviceFeatures::DepthClamp;
#endif
		m_supportedFeatures |= PhysicalDeviceFeatures::DepthBounds * false;
		m_supportedFeatures |= PhysicalDeviceFeatures::DepthBiasClamp;
#if PLATFORM_APPLE_IOS && !PLATFORM_APPLE_MACCATALYST
		m_supportedFeatures |= PhysicalDeviceFeatures::CubemapArrays * [mtlDevice supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily4_v1];
		m_supportedFeatures |= PhysicalDeviceFeatures::CubemapReadWrite * [mtlDevice supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily4_v1];
#else
		m_supportedFeatures |= PhysicalDeviceFeatures::CubemapArrays;
#endif
		m_supportedFeatures |= PhysicalDeviceFeatures::NonSolidFillMode;

#if PLATFORM_APPLE_IOS && !PLATFORM_APPLE_MACCATALYST
		m_supportedFeatures |= PhysicalDeviceFeatures::LayeredRendering * [mtlDevice supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily5_v1];
#else
		m_supportedFeatures |= PhysicalDeviceFeatures::LayeredRendering;
#endif

		m_supportedFeatures |= PhysicalDeviceFeatures::ReadWriteBuffers * [mtlDevice supportsFamily:MTLGPUFamilyApple3];
		m_supportedFeatures |= PhysicalDeviceFeatures::ReadWriteTextures * [mtlDevice supportsFamily:MTLGPUFamilyApple4];

#if ADVERTISE_RAYTRACING_SUPPORT
		if (@available(macOS 14.0, iOS 17.0, *))
		{
			if ([mtlDevice supportsFamily:MTLGPUFamilyApple6])
			{
				m_supportedFeatures |= PhysicalDeviceFeatures::AccelerationStructure;
				m_supportedFeatures |= PhysicalDeviceFeatures::RayQuery;
				m_supportedFeatures |= PhysicalDeviceFeatures::PartiallyBoundDescriptorBindings;
				m_supportedFeatures |= PhysicalDeviceFeatures::BufferDeviceAddress;
				m_supportedFeatures |= PhysicalDeviceFeatures::UpdateDescriptorSampleImageAfterBind;
				m_supportedFeatures |= PhysicalDeviceFeatures::NonUniformImageArrayIndexing;
				m_supportedFeatures |= PhysicalDeviceFeatures::RuntimeDescriptorArrays;
			}
		}
#endif

#elif RENDERER_WEBGPU
		Rendering::Window::ExecuteImmediatelyOnWindowThread(
			[this]()
			{
				m_supportedFeatures |= PhysicalDeviceFeatures::DepthClamp * (bool)wgpuAdapterHasFeature(m_device, WGPUFeatureName_DepthClipControl);
				m_supportedFeatures |= PhysicalDeviceFeatures::TextureCompressionBC *
			                         (bool)wgpuAdapterHasFeature(m_device, WGPUFeatureName_TextureCompressionBC);
				m_supportedFeatures |= PhysicalDeviceFeatures::TextureCompressionETC2 *
			                         (bool)wgpuAdapterHasFeature(m_device, WGPUFeatureName_TextureCompressionETC2);
				m_supportedFeatures |= PhysicalDeviceFeatures::TextureCompressionASTC_LDR *
			                         (bool)wgpuAdapterHasFeature(m_device, WGPUFeatureName_TextureCompressionASTC);
				m_supportedFeatures |= PhysicalDeviceFeatures::ShaderFloat16 * (bool)wgpuAdapterHasFeature(m_device, WGPUFeatureName_ShaderF16);
				m_supportedFeatures |= PhysicalDeviceFeatures::CubemapArrays;
				m_supportedFeatures |= PhysicalDeviceFeatures::CubemapReadWrite;
				m_supportedFeatures |= PhysicalDeviceFeatures::LayeredRendering;
			}
		);
#endif

#if RENDERER_VULKAN
		{
			QueueFamilyIndex numQueueFamilies;
			vkGetPhysicalDeviceQueueFamilyProperties(device, &numQueueFamilies, nullptr);

			FixedSizeVector<VkQueueFamilyProperties, QueueFamilyIndex>
				queueFamilies(Memory::ConstructWithSize, Memory::Uninitialized, numQueueFamilies);
			vkGetPhysicalDeviceQueueFamilyProperties(device, &numQueueFamilies, queueFamilies.GetData());
			m_queueFamilyCount = static_cast<QueueFamilyIndex>(numQueueFamilies);

			for (QueueFamilyIndex queueFamilyIndex = 0; queueFamilyIndex < numQueueFamilies; ++queueFamilyIndex)
			{
				const VkQueueFamilyProperties& __restrict queueFamily = queueFamilies[queueFamilyIndex];

				if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
				{
					AddQueueFamily(QueueFamily::Graphics, queueFamilyIndex);
					AddQueueFamily(QueueFamily::Transfer, queueFamilyIndex);
				}
				else if (queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT)
				{
					AddQueueFamily(QueueFamily::Transfer, queueFamilyIndex);
				}

				if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT)
				{
					AddQueueFamily(QueueFamily::Compute, queueFamilyIndex);
				}
			}

			{
				static_assert(VK_MAX_MEMORY_TYPES == MaximumMemoryTypeCount);

				VkPhysicalDeviceMemoryProperties memProperties;
				vkGetPhysicalDeviceMemoryProperties(device, &memProperties);
				Assert(memProperties.memoryTypeCount <= m_memoryTypes.GetCapacity());

				static_assert(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT == (uint8)MemoryFlags::DeviceLocal);
				static_assert(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT == (uint8)MemoryFlags::HostVisible);
				static_assert(VK_MEMORY_PROPERTY_HOST_COHERENT_BIT == (uint8)MemoryFlags::HostCoherent);
				static_assert(VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT == (uint8)MemoryFlags::LazilyAllocated);
				static_assert(VK_MEMORY_PROPERTY_PROTECTED_BIT == (uint8)MemoryFlags::Protected);

				m_memoryTypes.Resize(static_cast<uint8>(memProperties.memoryTypeCount));
				for (uint8 i = 0; i < memProperties.memoryTypeCount; i++)
				{
					m_memoryTypes[i] = static_cast<MemoryFlags>(memProperties.memoryTypes[i].propertyFlags) | MemoryFlags::AllocateDeviceAddress;
				}
			}
		}
#elif RENDERER_METAL
		// TODO: All Metal command queues are general purpose by default
		// We could imply that we have more than one.
		m_queueFamilyCount = 1;
		AddQueueFamily(QueueFamily::Graphics, 0);
		AddQueueFamily(QueueFamily::Transfer, 0);
		AddQueueFamily(QueueFamily::Compute, 0);

		m_memoryTypes.Resize(4);
		m_memoryTypes.EmplaceBack(
			MemoryFlags::DeviceLocal | MemoryFlags::LazilyAllocated | MemoryFlags::AllocateDeviceAddress
		);                                                                                        // MTLStorageModeMemoryless
		m_memoryTypes.EmplaceBack(MemoryFlags::DeviceLocal | MemoryFlags::AllocateDeviceAddress); // MTLStorageModePrivate
		m_memoryTypes.EmplaceBack(
			MemoryFlags::HostVisible | MemoryFlags::HostCoherent | MemoryFlags::AllocateDeviceAddress
		);                                                                                        // MTLStorageModeShared
		m_memoryTypes.EmplaceBack(MemoryFlags::HostVisible | MemoryFlags::AllocateDeviceAddress); // MTLStorageModeManaged

#elif RENDERER_WEBGPU
		m_queueFamilyCount = 1;
		AddQueueFamily(QueueFamily::Graphics, 0);
		AddQueueFamily(QueueFamily::Transfer, 0);
		AddQueueFamily(QueueFamily::Compute, 0);
#endif
	}

	PhysicalDevice::~PhysicalDevice()
	{
#if RENDERER_WEBGPU
		wgpuAdapterRelease(m_device);
#endif
	}

	[[nodiscard]] PURE_LOCALS_AND_POINTERS uint8
	PhysicalDevice::GetMemoryTypeIndex(const EnumFlags<MemoryFlags> memoryFlags, const uint32 typeFilter) const
	{
		const RestrictedArrayView<const EnumFlags<MemoryFlags>> memoryTypes = m_memoryTypes.GetView();
		for (const uint8 memoryTypeIndex : Memory::GetSetBitsIterator(typeFilter))
		{
			const EnumFlags<MemoryFlags> memoryTypeFlags = memoryTypes[memoryTypeIndex];
			if (memoryTypeFlags.AreAllSet(memoryFlags))
			{
				return memoryTypeIndex;
			}
		}

		Assert(false, "Requested memory type was not registered!");
		return Math::NumericLimits<uint8>::Max;
	}
}
