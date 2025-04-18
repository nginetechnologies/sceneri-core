#pragma once

#include <Common/EnumFlags.h>

#include "ImageView.h"
#include "ImageMappingView.h"
#include "ImageMappingType.h"

#include <Renderer/Constants.h>
#include <Renderer/Format.h>
#include <Renderer/ImageAspectFlags.h>
#include <Renderer/Assets/Texture/MipRange.h>
#include <Renderer/Assets/Texture/ArrayRange.h>

#include <Common/Memory/Containers/ForwardDeclarations/ZeroTerminatedStringView.h>

namespace ngine::Rendering
{
#if RENDERER_METAL
	namespace Internal
	{
		struct ImageMappingData
		{
			id<MTLTexture> m_textureView;
		};
	}
#elif RENDERER_WEBGPU
	namespace Internal
	{
		struct ImageMappingData
		{
			WGPUTextureView m_textureView;
		};
	}
#endif

	struct LogicalDeviceView;
	struct LogicalDevice;
	struct ImageView;

	struct ImageMapping : public ImageMappingView
	{
		using Type = ImageMappingType;

		ImageMapping()
		{
		}
		ImageMapping(
			const LogicalDeviceView logicalDevice,
			const ImageView image,
			const Type type,
			const Format format,
			const EnumFlags<ImageAspectFlags> aspectFlags,
			const MipRange mipRange = {0, 1},
			const ArrayRange arrayLevelRange = {0, 1}
		);
		ImageMapping(const ImageMapping&) = delete;
		ImageMapping& operator=(const ImageMapping&) = delete;
		ImageMapping([[maybe_unused]] ImageMapping&& other)
#if RENDERER_VULKAN || RENDERER_METAL || RENDERER_WEBGPU
			: ImageMappingView(other.m_pImageView)
#endif
		{
#if RENDERER_VULKAN || RENDERER_METAL || RENDERER_WEBGPU
			other.m_pImageView = 0;
#endif
		}
		ImageMapping& operator=(ImageMapping&& other);
		~ImageMapping();

		[[nodiscard]] ImageMappingView AtomicLoad() const;
		void AtomicSwap(ImageMapping& other);

		void SetDebugName(const LogicalDevice& logicalDevice, const ConstZeroTerminatedStringView name);

		void Destroy(const LogicalDeviceView logicalDevice);
	};
}
