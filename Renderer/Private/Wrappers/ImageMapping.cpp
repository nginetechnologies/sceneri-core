#include "Wrappers/ImageMapping.h"
#include "Wrappers/ImageView.h"

#include <Renderer/Devices/LogicalDevice.h>
#include <Renderer/Vulkan/Includes.h>
#include <Renderer/WebGPU/Includes.h>
#include <WebGPU/ConvertFormat.h>
#include <WebGPU/ConvertImageAspectFlags.h>
#include <WebGPU/ConvertImageMappingType.h>
#include <Renderer/Metal/ConvertFormat.h>
#include <Renderer/Window/Window.h>

#include <Renderer/Devices/LogicalDeviceView.h>
#include <Common/Assert/Assert.h>
#include <Common/Threading/Atomics/Exchange.h>
#include <Common/Threading/Atomics/Load.h>

#if RENDERER_METAL
#import <Foundation/NSString.h>
#endif

namespace ngine::Rendering
{
#if RENDERER_METAL
	[[nodiscard]] constexpr MTLTextureType GetTextureType(const ImageMappingType type)
	{
		switch (type)
		{
			case ImageMappingType::OneDimensional:
				return MTLTextureType1D;
			case ImageMappingType::TwoDimensional:
				return MTLTextureType2D;
			case ImageMappingType::ThreeDimensional:
				return MTLTextureType3D;
			case ImageMappingType::Cube:
				return MTLTextureTypeCube;
			case ImageMappingType::OneDimensionalArray:
				return MTLTextureType1DArray;
			case ImageMappingType::TwoDimensionalArray:
				return MTLTextureType2DArray;
			case ImageMappingType::CubeArray:
				return MTLTextureTypeCubeArray;
		}
		ExpectUnreachable();
	}
#endif

	ImageMapping::ImageMapping(
		[[maybe_unused]] const LogicalDeviceView logicalDevice,
		const ImageView image,
		const Type type,
		Format format,
		const EnumFlags<ImageAspectFlags> aspectFlags,
		const MipRange mipRange,
		const ArrayRange arrayLevelRange
	)
	{
		Assert(image.IsValid());
		Assert(
			arrayLevelRange.GetCount() == 1 || type == Type::OneDimensionalArray || type == Type::TwoDimensionalArray ||
			type == Type::CubeArray || (type == Type::Cube && arrayLevelRange.GetCount() == 6)
		);

#if RENDERER_VULKAN
		const VkImageViewCreateInfo viewInfo = {
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			nullptr,
			0,
			image,
			static_cast<VkImageViewType>(type),
			static_cast<VkFormat>(format),
			VkComponentMapping{
				VK_COMPONENT_SWIZZLE_IDENTITY,
				VK_COMPONENT_SWIZZLE_IDENTITY,
				VK_COMPONENT_SWIZZLE_IDENTITY,
				VK_COMPONENT_SWIZZLE_IDENTITY
			},
			VkImageSubresourceRange{
				aspectFlags.GetUnderlyingValue(),
				mipRange.GetIndex(),
				mipRange.GetCount(),
				arrayLevelRange.GetIndex(),
				arrayLevelRange.GetCount()
			}
		};

		[[maybe_unused]] const VkResult result = vkCreateImageView(logicalDevice, &viewInfo, nullptr, &m_pImageView);
		Assert(result == VK_SUCCESS);
#elif RENDERER_METAL
		UNUSED(aspectFlags);

		const MTLTextureType textureType = [](const Type type)
		{
			switch (type)
			{
				case Type::OneDimensional:
					return MTLTextureType1D;
				case Type::TwoDimensional:
					return MTLTextureType2D;
				case Type::ThreeDimensional:
					return MTLTextureType3D;
				case Type::Cube:
					return MTLTextureTypeCube;
				case Type::OneDimensionalArray:
					return MTLTextureType1DArray;
				case Type::TwoDimensionalArray:
					return MTLTextureType2DArray;
				case Type::CubeArray:
					return MTLTextureTypeCubeArray;
			}
			ExpectUnreachable();
		}(type);

		id<MTLTexture> mtlTextureView = nil;
		const id<MTLTexture> mtlTexture = image;
		if (mtlTexture != nil)
		{
			mtlTextureView = [mtlTexture newTextureViewWithPixelFormat:ConvertFormat(format)
																										 textureType:textureType
																													levels:NSMakeRange(mipRange.GetIndex(), mipRange.GetCount())
																													slices:NSMakeRange(arrayLevelRange.GetIndex(), arrayLevelRange.GetCount())];
			Assert(mtlTextureView != nil);
		}
		m_pImageView = new Internal::ImageMappingData{mtlTextureView};
#elif RENDERER_WEBGPU
		if (aspectFlags == ImageAspectFlags::Depth)
		{
			switch (format)
			{
				case Format::D16_UNORM_S8_UINT:
					format = Format::D16_UNORM;
					break;
				case Format::D24_UNORM_S8_UINT:
					format = Format::D24_UNORM;
					break;
				case Format::D32_SFLOAT_S8_UINT:
					format = Format::D32_SFLOAT;
					break;
				default:
					break;
			}
		}
		else if (aspectFlags == ImageAspectFlags::Stencil)
		{
			switch (format)
			{
				case Format::D16_UNORM_S8_UINT:
				case Format::D24_UNORM_S8_UINT:
				case Format::D32_SFLOAT_S8_UINT:
					format = Format::S8_UINT;
					break;
				default:
					break;
			}
		}

		const WGPUTextureViewDescriptor textureViewDescriptor{
			nullptr,
#if RENDERER_WEBGPU_DAWN
			WGPUStringView { nullptr, 0 },
#else
			nullptr,
#endif
			ConvertFormat(format),
			ConvertImageMappingType(type),
			mipRange.GetIndex(),
			mipRange.GetCount(),
			arrayLevelRange.GetIndex(),
			arrayLevelRange.GetCount(),
			ConvertImageAspectFlags(aspectFlags)
		};

		Internal::ImageMappingData* pImageMappingData = new Internal::ImageMappingData{nullptr};
#if WEBGPU_SINGLE_THREADED
		Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
			[image, textureViewDescriptor, pImageMappingData]()
			{
				const WGPUTexture pTexture = image;
				WGPUTextureView pTextureView = nullptr;
				if (pTexture != nullptr)
				{
					pTextureView = wgpuTextureCreateView(pTexture, &textureViewDescriptor);
#if RENDERER_WEBGPU_DAWN
					wgpuTextureViewAddRef(pTextureView);
#else
					wgpuTextureViewReference(pTextureView);
#endif
					pImageMappingData->m_textureView = pTextureView;
				}
			}
		);
#else
		const WGPUTexture pTexture = image;
		WGPUTextureView pTextureView = nullptr;
		if (pTexture != nullptr)
		{
			pTextureView = wgpuTextureCreateView(pTexture, &textureViewDescriptor);
#if RENDERER_WEBGPU_DAWN
			wgpuTextureViewAddRef(pTextureView);
#else
			wgpuTextureViewReference(pTextureView);
#endif
			pImageMappingData->m_textureView = pTextureView;
		}
#endif
		m_pImageView = pImageMappingData;
#else
		UNUSED(logicalDevice);
		UNUSED(image);
		UNUSED(type);
		UNUSED(format);
		UNUSED(aspectFlags);
		UNUSED(mipRange);
		UNUSED(arrayLevelRange);
#endif
	}

	ImageMapping& ImageMapping::operator=([[maybe_unused]] ImageMapping&& other)
	{
		Assert(!IsValid(), "Destroy must have been called!");
#if RENDERER_VULKAN || RENDERER_METAL || RENDERER_WEBGPU
		m_pImageView = other.m_pImageView;
		other.m_pImageView = 0;
#endif
		return *this;
	}

	ImageMapping::~ImageMapping()
	{
		Assert(!IsValid(), "Destroy must have been called!");
	}

	void ImageMapping::AtomicSwap([[maybe_unused]] ImageMapping& other)
	{
#if RENDERER_VULKAN || RENDERER_METAL || RENDERER_WEBGPU
		other.m_pImageView = Threading::Atomics::Exchange(m_pImageView, other.m_pImageView);
#endif
	}

	ImageMappingView ImageMapping::AtomicLoad() const
	{
#if RENDERER_VULKAN || RENDERER_METAL || RENDERER_WEBGPU
		return Threading::Atomics::Load(m_pImageView);
#else
		return {};
#endif
	}

	void ImageMapping::Destroy([[maybe_unused]] const LogicalDeviceView logicalDevice)
	{
#if RENDERER_VULKAN
		vkDestroyImageView(logicalDevice, m_pImageView, nullptr);
		m_pImageView = 0;
#elif RENDERER_METAL
		delete m_pImageView;
		m_pImageView = nullptr;
#elif RENDERER_WEBGPU
		if (m_pImageView != nullptr)
		{
			Internal::ImageMappingData* pImageMappingData = m_pImageView;
#if WEBGPU_SINGLE_THREADED
			Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
				[pImageMappingData]()
				{
					if (pImageMappingData->m_textureView != nullptr)
					{
						wgpuTextureViewRelease(pImageMappingData->m_textureView);
					}
					delete pImageMappingData;
				}
			);
#else
			if (pImageMappingData->m_textureView != nullptr)
			{
				wgpuTextureViewRelease(pImageMappingData->m_textureView);
			}
			delete pImageMappingData;
#endif
			m_pImageView = nullptr;
		}
#endif
	}

#if RENDERER_METAL
	ImageMappingView::operator id<MTLTexture>() const
	{
		return m_pImageView->m_textureView;
	}
#elif RENDERER_WEBGPU
	ImageMappingView::operator WGPUTextureView() const
	{
		return m_pImageView->m_textureView;
	}
#endif

	void
	ImageMapping::SetDebugName([[maybe_unused]] const LogicalDevice& logicalDevice, [[maybe_unused]] const ConstZeroTerminatedStringView name)
	{
#if RENDERER_VULKAN
		const VkDebugUtilsObjectNameInfoEXT debugInfo{
			VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
			nullptr,
			VK_OBJECT_TYPE_IMAGE_VIEW,
			reinterpret_cast<uint64_t>(m_pImageView),
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
		if (m_pImageView->m_textureView != nil)
		{
			[m_pImageView->m_textureView setLabel:[NSString stringWithUTF8String:name]];
		}

#elif RENDERER_WEBGPU
#if WEBGPU_SINGLE_THREADED
		Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
			[pImageMappingData = m_pImageView, name]()
			{
				if (pImageMappingData->m_textureView != nullptr)
				{
#if RENDERER_WEBGPU_DAWN
					wgpuTextureViewSetLabel(pImageMappingData->m_textureView, WGPUStringView { name, name.GetSize() });
#else
					wgpuTextureViewSetLabel(pImageMappingData->m_textureView, name);
#endif
				}
			}
		);
#else
		if (m_pImageView->m_textureView != nullptr)
		{
			wgpuTextureViewSetLabel(m_pImageView->m_textureView, name);
		}
#endif
#else
		Assert(false, "TODO");
		UNUSED(logicalDevice);
		UNUSED(name);
#endif
	}
}
