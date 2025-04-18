#pragma once

#include "ImageView.h"

#include <Renderer/UsageFlags.h>
#include <Renderer/ImageLayout.h>
#include <Renderer/Format.h>
#include <Renderer/SampleCount.h>
#include <Renderer/Wrappers/ImageFlags.h>
#include <Renderer/Assets/Texture/ArrayRange.h>
#include <Renderer/Assets/Texture/MipRange.h>

#include <Renderer/Devices/DeviceMemoryAllocation.h>

#include <Common/Math/ForwardDeclarations/Vector2.h>
#include <Common/Math/ForwardDeclarations/Vector3.h>
#include <Common/EnumFlags.h>

#if RENDERER_OBJECT_DEBUG_NAMES
#include <Common/Memory/Containers/ForwardDeclarations/ZeroTerminatedStringView.h>
#include <Common/Memory/Containers/String.h>
#endif

namespace ngine::Rendering
{
#if RENDERER_METAL
	namespace Internal
	{
		struct ImageData
		{
			[[nodiscard]] operator id<MTLTexture>() const
			{
				return m_image;
			}

			id<MTLTexture> m_image;
		};
	}
#elif RENDERER_WEBGPU
	namespace Internal
	{
		struct ImageData
		{
			[[nodiscard]] operator WGPUTexture() const
			{
				return m_image;
			}

			WGPUTexture m_image;
		};
	}
#endif

	struct LogicalDevice;
	struct LogicalDeviceView;
	struct PhysicalDevice;
	struct DeviceMemoryPool;

	struct Image : public ImageView
	{
		using Flags = ImageFlags;

		Image() = default;
		Image(
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
		);
#if RENDERER_METAL
		Image(Internal::ImageData* pImageData)
			: ImageView(pImageData)
		{
		}
#endif
		Image(const Image&) = delete;
		Image& operator=(const Image&) = delete;
		Image(Image&& other)
			: ImageView(other)
#if RENDERER_HAS_DEVICE_MEMORY
			, m_deviceMemoryAllocation(other.m_deviceMemoryAllocation)
#endif
#if RENDERER_OBJECT_DEBUG_NAMES
			, m_debugName(Move(other.m_debugName))
#endif
		{
#if RENDERER_VULKAN || RENDERER_METAL || RENDERER_WEBGPU
			other.m_pImage = 0;
#endif
#if RENDERER_HAS_DEVICE_MEMORY
			other.m_deviceMemoryAllocation.m_memory = {};
#endif
		}
		Image& operator=(Image&& other);
		~Image();

		void Destroy(const LogicalDeviceView logicalDevice, DeviceMemoryPool& memoryPool);

		[[nodiscard]] bool IsValid() const
		{
#if RENDERER_HAS_DEVICE_MEMORY
			return ImageView::IsValid() & m_deviceMemoryAllocation.IsValid();
#else
			return ImageView::IsValid();
#endif
		}

#if RENDERER_OBJECT_DEBUG_NAMES
		void SetDebugName(const LogicalDevice& logicalDevice, const ConstZeroTerminatedStringView name);
		[[nodiscard]] ConstZeroTerminatedStringView GetDebugName() const
		{
			return m_debugName;
		}
#endif
	private:
		void CreateImage(
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
		);
	private:
#if RENDERER_HAS_DEVICE_MEMORY
		DeviceMemoryAllocation m_deviceMemoryAllocation;
#endif

#if RENDERER_OBJECT_DEBUG_NAMES
		String m_debugName;
#endif
	};
}
