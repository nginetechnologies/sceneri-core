#include "Wrappers/Image.h"

#include <Renderer/Vulkan/Includes.h>
#include <Renderer/Metal/Includes.h>
#include <Metal/ConvertFormat.h>
#include <Renderer/WebGPU/Includes.h>
#include <WebGPU/ConvertFormat.h>

#include <Renderer/Devices/LogicalDevice.h>
#include <Renderer/Devices/PhysicalDevice.h>
#include <Renderer/Devices/MemoryFlags.h>
#include <Renderer/Commands/CommandEncoderView.h>
#include <Renderer/Window/Window.h>

#include <Common/Assert/Assert.h>
#include <Common/Math/Vector2.h>
#include <Common/Math/Vector3.h>
#include <Common/Memory/Align.h>

namespace ngine::Rendering
{
#if RENDERER_VULKAN
	static_assert((uint32)UsageFlags::TransferSource == VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
	static_assert((uint32)UsageFlags::TransferDestination == VK_IMAGE_USAGE_TRANSFER_DST_BIT);
	static_assert((uint32)UsageFlags::Sampled == VK_IMAGE_USAGE_SAMPLED_BIT);
	static_assert((uint32)UsageFlags::Storage == VK_IMAGE_USAGE_STORAGE_BIT);
	static_assert((uint32)UsageFlags::ColorAttachment == VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
	static_assert((uint32)UsageFlags::DepthStencilAttachment == VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
#endif

	Image::Image(
		const LogicalDeviceView logicalDevice,
		const PhysicalDevice& physicalDevice,
		DeviceMemoryPool& memoryPool,
		const Format format,
		const SampleCount sampleCount,
		const EnumFlags<Flags> flags,
		const Math::Vector3ui resolution,
		const EnumFlags<UsageFlags> usageFlags,
		const ImageLayout initialLayout,
		const MipRange::UnitType numMips,
		const ArrayRange::UnitType numArrayLayers
	)
	{
		CreateImage(
			logicalDevice,
			physicalDevice,
			memoryPool,
			format,
			sampleCount,
			flags,
			resolution,
			usageFlags,
			initialLayout,
			numMips,
			numArrayLayers
		);
	}

	Image::~Image()
	{
#if RENDERER_VULKAN || RENDERER_METAL || RENDERER_WEBGPU
		Assert(m_pImage == 0, "Destroy must have been called!");
#endif
	}

	Image& Image::operator=(Image&& other)
	{
#if RENDERER_VULKAN || RENDERER_METAL || RENDERER_WEBGPU
		Assert(m_pImage == 0, "Destroy must have been called!");
		m_pImage = other.m_pImage;
		other.m_pImage = 0;
#endif

#if RENDERER_HAS_DEVICE_MEMORY
		Assert(!m_deviceMemoryAllocation.m_memory.IsValid(), "Destroy must have been called!");
		m_deviceMemoryAllocation = other.m_deviceMemoryAllocation;
		other.m_deviceMemoryAllocation.m_memory = {};
#endif

#if RENDERER_OBJECT_DEBUG_NAMES
		m_debugName = Move(other.m_debugName);
#endif
		return *this;
	}

	void Image::Destroy([[maybe_unused]] const LogicalDeviceView logicalDevice, [[maybe_unused]] DeviceMemoryPool& memoryPool)
	{
#if RENDERER_VULKAN
		vkDestroyImage(logicalDevice, m_pImage, nullptr);
		m_pImage = 0;
#elif RENDERER_METAL
		delete m_pImage;
		m_pImage = nullptr;
#elif RENDERER_WEBGPU
		if (m_pImage != nullptr)
		{
			Internal::ImageData* pImageData = m_pImage;
#if WEBGPU_SINGLE_THREADED
			Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
				[pImageData]()
				{
					if (pImageData->m_image != nullptr)
					{
						wgpuTextureRelease(pImageData->m_image);
						wgpuTextureDestroy(pImageData->m_image);
					}
				}
			);
#else
			if (pImageData->m_image != nullptr)
			{
				wgpuTextureRelease(pImageData->m_image);
				wgpuTextureDestroy(pImageData->m_image);
			}
			delete pImageData;
#endif
			m_pImage = nullptr;
		}
#endif

#if RENDERER_HAS_DEVICE_MEMORY
		if (m_deviceMemoryAllocation.m_memory.IsValid())
		{
#if DISABLE_MEMORY_POOL_USAGE
			memoryPool.DeallocateRaw(logicalDevice, m_deviceMemoryAllocation);
#else
			memoryPool.Deallocate(m_deviceMemoryAllocation);
#endif
			m_deviceMemoryAllocation.m_memory = {};
		}
#endif
	}

#if RENDERER_METAL
	[[nodiscard]] inline constexpr MTLTextureUsage ConvertUsageFlags(const EnumFlags<UsageFlags> usageFlags)
	{
		MTLTextureUsage textureUsage = MTLTextureUsageUnknown;

		// Read from...
		textureUsage |=
			MTLTextureUsageShaderRead *
			usageFlags.AreAnySet(UsageFlags::TransferSource | UsageFlags::Sampled | UsageFlags::Storage | UsageFlags::InputAttachment);

		textureUsage |= MTLTextureUsageShaderWrite * usageFlags.IsSet(UsageFlags::Storage);

		textureUsage |= MTLTextureUsageRenderTarget * usageFlags.AreAnySet(UsageFlags::ColorAttachment | UsageFlags::DepthStencilAttachment);
		return textureUsage;
	}
#endif

	void Image::CreateImage(
		const LogicalDeviceView logicalDevice,
		[[maybe_unused]] const PhysicalDevice& physicalDevice,
		[[maybe_unused]] DeviceMemoryPool& memoryPool,
		const Format format,
		const SampleCount sampleCount,
		[[maybe_unused]] const EnumFlags<Flags> flags,
		const Math::Vector3ui resolution,
		const EnumFlags<UsageFlags> usageFlags,
		[[maybe_unused]] const ImageLayout initialLayout,
		const MipRange::UnitType numMips,
		const ArrayRange::UnitType numArrayLayers
	)
	{
#if RENDERER_VULKAN || RENDERER_METAL || RENDERER_WEBGPU
		Assert(m_pImage == 0, "Destroy must have been called!");
#endif

#if RENDERER_VULKAN
		const VkImageCreateInfo imageCreationInfo = {
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
			nullptr,
			static_cast<VkImageCreateFlags>(flags.GetUnderlyingValue()),
			VK_IMAGE_TYPE_2D,
			static_cast<VkFormat>(format),
			VkExtent3D{resolution.x, resolution.y, resolution.z},
			numMips,
			numArrayLayers,
			static_cast<VkSampleCountFlagBits>(sampleCount),
			VK_IMAGE_TILING_OPTIMAL,
			static_cast<VkImageUsageFlags>(usageFlags.GetUnderlyingValue()),
			VK_SHARING_MODE_EXCLUSIVE,
			0,
			nullptr,
			static_cast<VkImageLayout>(initialLayout)
		};

		// Validate the texture format
		{
			VkImageFormatProperties props;
			const VkResult result = vkGetPhysicalDeviceImageFormatProperties(
				physicalDevice,
				imageCreationInfo.format,
				imageCreationInfo.imageType,
				imageCreationInfo.tiling,
				imageCreationInfo.usage,
				imageCreationInfo.flags,
				&props
			);
			if(UNLIKELY(result != VK_SUCCESS || imageCreationInfo.arrayLayers > props.maxArrayLayers) ||
			   imageCreationInfo.extent.width > props.maxExtent.width || imageCreationInfo.extent.height > props.maxExtent.height ||
			   imageCreationInfo.extent.depth > props.maxExtent.depth || imageCreationInfo.mipLevels > props.maxMipLevels ||
			   imageCreationInfo.samples > static_cast<VkSampleCountFlagBits>(props.sampleCounts))
			{
				Assert(false);
				return;
			}
		}

		const VkResult result = vkCreateImage(logicalDevice, &imageCreationInfo, nullptr, &m_pImage);
		Assert(result == VK_SUCCESS);
		if (LIKELY(result == VK_SUCCESS))
		{
			{
				VkMemoryRequirements memoryRequirements;
				vkGetImageMemoryRequirements(logicalDevice, m_pImage, &memoryRequirements);

				const uint8 memoryTypeIndex = physicalDevice.GetMemoryTypeIndex(MemoryFlags::DeviceLocal, memoryRequirements.memoryTypeBits);

#if DISABLE_MEMORY_POOL_USAGE
				m_deviceMemoryAllocation = memoryPool.AllocateRaw(logicalDevice, size)memoryRequirements.size, memoryTypeIndex);
#else
				m_deviceMemoryAllocation = memoryPool.Allocate(
					logicalDevice,
					(size)memoryRequirements.size,
					(uint32)memoryRequirements.alignment,
					memoryTypeIndex,
					MemoryFlags::DeviceLocal
				);
#endif
				if (LIKELY(m_deviceMemoryAllocation.IsValid()))
				{
					[[maybe_unused]] const VkResult bindResult = vkBindImageMemory(
						logicalDevice,
						m_pImage,
						m_deviceMemoryAllocation.m_memory,
						Memory::Align(m_deviceMemoryAllocation.m_offset, (size)memoryRequirements.alignment)
					);
					Assert(bindResult == VK_SUCCESS);
				}
			}
		}
#elif RENDERER_METAL
		MTLTextureDescriptor* textureDescriptor = [MTLTextureDescriptor new];
		textureDescriptor.pixelFormat = ConvertFormat(format);
		if (flags.IsSet(Flags::Cubemap))
		{
			textureDescriptor.textureType = MTLTextureTypeCube;
		}
		else if (numArrayLayers > 1)
		{
			textureDescriptor.textureType = MTLTextureType2DArray;
		}
		else
		{
			textureDescriptor.textureType = MTLTextureType2D;
		}
		textureDescriptor.width = resolution.x;
		textureDescriptor.height = resolution.y;
		textureDescriptor.depth = resolution.z;
		textureDescriptor.mipmapLevelCount = numMips;
		textureDescriptor.sampleCount = static_cast<uint32>(sampleCount);
		textureDescriptor.arrayLength = flags.IsSet(Flags::Cubemap) ? 1 : numArrayLayers;
		textureDescriptor.usage = ConvertUsageFlags(usageFlags);
		textureDescriptor.storageMode = usageFlags.IsSet(UsageFlags::TransientAttachment) ? MTLStorageModeMemoryless : MTLStorageModePrivate;
		textureDescriptor.cpuCacheMode = MTLCPUCacheModeDefaultCache;

		const MTLSizeAndAlign sizeAndAlign = [(id<MTLDevice>)logicalDevice heapTextureSizeAndAlignWithDescriptor:textureDescriptor];

		const uint8 memoryTypeIndex = physicalDevice.GetMemoryTypeIndex(MemoryFlags::DeviceLocal);
		m_deviceMemoryAllocation =
			memoryPool.Allocate(logicalDevice, sizeAndAlign.size, (uint32)sizeAndAlign.align, memoryTypeIndex, MemoryFlags::DeviceLocal);

		id<MTLTexture> texture = [(id<MTLHeap>)m_deviceMemoryAllocation.m_memory
			newTextureWithDescriptor:(MTLTextureDescriptor*)textureDescriptor
												offset:Memory::Align(m_deviceMemoryAllocation.m_offset, sizeAndAlign.align)];

		m_pImage = new Internal::ImageData{texture};
#elif RENDERER_WEBGPU
		UNUSED(physicalDevice);
		Assert(resolution.z == 1, "Unsupported");

		uint32 wgpuUsageFlags{0};
		wgpuUsageFlags = WGPUTextureUsage_RenderAttachment *
		                 usageFlags.AreAnySet(UsageFlags::InputAttachment | UsageFlags::ColorAttachment | UsageFlags::DepthStencilAttachment);
		wgpuUsageFlags |= WGPUTextureUsage_CopySrc * usageFlags.IsSet(UsageFlags::TransferSource);
		wgpuUsageFlags |= WGPUTextureUsage_CopyDst * usageFlags.IsSet(UsageFlags::TransferDestination);
		wgpuUsageFlags |= WGPUTextureUsage_TextureBinding * usageFlags.IsSet(UsageFlags::Sampled);
		wgpuUsageFlags |= WGPUTextureUsage_StorageBinding * usageFlags.IsSet(UsageFlags::Storage);
		// wgpuUsageFlags |= WGPUTextureUsage_Transient * usageFlags.IsSet(UsageFlags::TransientAttachment);
		Assert(wgpuUsageFlags != 0);

		const WGPUTextureDescriptor descriptor
		{
			nullptr,
#if RENDERER_WEBGPU_DAWN
				WGPUStringView{nullptr, 0},
#else
				nullptr,
#endif
				wgpuUsageFlags, WGPUTextureDimension_2D, WGPUExtent3D{resolution.x, resolution.y, numArrayLayers}, ConvertFormat(format), numMips,
				static_cast<uint32>(sampleCount),
#if RENDERER_WEBGPU_DAWN
				0, nullptr
#endif
		};
		Assert(descriptor.format != WGPUTextureFormat_Undefined);
		Internal::ImageData* pImageData = new Internal::ImageData{nullptr};
#if WEBGPU_SINGLE_THREADED
		Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
			[logicalDevice, descriptor, pImageData]()
			{
				WGPUTexture pTexture = wgpuDeviceCreateTexture(logicalDevice, &descriptor);
#if RENDERER_WEBGPU_DAWN
				wgpuTextureAddRef(pTexture);
#else
				wgpuTextureReference(pTexture);
#endif
				pImageData->m_image = pTexture;
			}
		);
#else
		WGPUTexture pTexture = wgpuDeviceCreateTexture(logicalDevice, &descriptor);
#if RENDERER_WEBGPU_DAWN
		wgpuTextureAddRef(pTexture);
#else
		wgpuTextureReference(pTexture);
#endif
		pImageData->m_image = pTexture;
#endif
		m_pImage = pImageData;

		Assert(initialLayout == ImageLayout::Undefined);
#else
		UNUSED(physicalDevice);
		UNUSED(format);
		UNUSED(sampleCount);
		UNUSED(resolution);
		UNUSED(usageFlags);
		UNUSED(numMips);
		UNUSED(numArrayLayers);
#endif
	}

	void ImageView::SetDebugName([[maybe_unused]] const LogicalDevice& logicalDevice, const ConstZeroTerminatedStringView name)
	{
#if RENDERER_VULKAN
		const VkDebugUtilsObjectNameInfoEXT debugInfo{
			VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
			nullptr,
			VK_OBJECT_TYPE_IMAGE,
			reinterpret_cast<uint64_t>(m_pImage),
			name
		};

#if PLATFORM_APPLE
		vkSetDebugUtilsObjectNameEXT(logicalDevice, &debugInfo);
#else
		const PFN_vkSetDebugUtilsObjectNameEXT setDebugUtilsObjectNameEXT =
			reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(logicalDevice.GetSetDebugUtilsObjectNameEXT());
		if (setDebugUtilsObjectNameEXT != nullptr)
		{
			setDebugUtilsObjectNameEXT(logicalDevice, &debugInfo);
		}
#endif

#elif RENDERER_METAL
		if (m_pImage->m_image != nil)
		{
			[(id<MTLTexture>)*m_pImage setLabel:[NSString stringWithUTF8String:name]];
		}

#elif RENDERER_WEBGPU
#if WEBGPU_SINGLE_THREADED
		Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
			[pImage = m_pImage, name]()
			{
				if (pImage->m_image != nullptr)
				{
#if RENDERER_WEBGPU_DAWN
					wgpuTextureSetLabel(pImage->m_image, WGPUStringView{name, name.GetSize()});
#else
					wgpuTextureSetLabel(pImage->m_image, name);
#endif
				}
			}
		);
#else
		if (m_pImage->m_image != nullptr)
		{
			wgpuTextureSetLabel(m_pImage->m_image, name);
		}
#endif
#else
		Assert(false, "TODO");
		UNUSED(logicalDevice);
		UNUSED(name);
#endif
	}

#if RENDERER_OBJECT_DEBUG_NAMES
	void Image::SetDebugName(const LogicalDevice& logicalDevice, const ConstZeroTerminatedStringView name)
	{
		m_debugName = name;
		ImageView::SetDebugName(logicalDevice, m_debugName);
	}
#endif

#if RENDERER_METAL
	ImageView::operator id<MTLTexture>() const
	{
		return *m_pImage;
	}
#elif RENDERER_WEBGPU
	ImageView::operator WGPUTexture() const
	{
		return *m_pImage;
	}
#endif
}
