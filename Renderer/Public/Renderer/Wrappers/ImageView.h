#pragma once

#include <Renderer/Constants.h>
#include <Renderer/Vulkan/ForwardDeclares.h>
#include <Renderer/Metal/ForwardDeclares.h>
#include <Renderer/WebGPU/ForwardDeclares.h>

#include <Common/Platform/ForceInline.h>
#include <Common/Platform/LifetimeBound.h>
#include <Common/Platform/TrivialABI.h>
#include <Common/Memory/Containers/ForwardDeclarations/ZeroTerminatedStringView.h>

namespace ngine::Rendering
{
	struct CommandEncoderView;
	struct LogicalDevice;

#if RENDERER_METAL || RENDERER_WEBGPU
	namespace Internal
	{
		struct ImageData;
	}
#endif

	struct TRIVIAL_ABI ImageView
	{
		ImageView() = default;

#if RENDERER_VULKAN
		ImageView(const VkImage pImage)
			: m_pImage(pImage)
		{
		}

		[[nodiscard]] operator VkImage() const
		{
			return m_pImage;
		}
#elif RENDERER_METAL || RENDERER_WEBGPU
		ImageView(Internal::ImageData* pImage)
			: m_pImage(pImage)
		{
		}

		[[nodiscard]] operator Internal::ImageData*() const
		{
			return m_pImage;
		}
#if RENDERER_METAL
		[[nodiscard]] operator id<MTLTexture>() const;
#elif RENDERER_WEBGPU
		[[nodiscard]] operator WGPUTexture() const;
#endif

#endif

		[[nodiscard]] bool IsValid() const
		{
#if RENDERER_VULKAN || RENDERER_METAL || RENDERER_WEBGPU
			return m_pImage != 0;
#else
			return false;
#endif
		}

		[[nodiscard]] bool operator==(const ImageView other) const
		{
#if RENDERER_VULKAN || RENDERER_METAL || RENDERER_WEBGPU
			return m_pImage == other.m_pImage;
#else
			return false;
#endif
		}

		void SetDebugName(const LogicalDevice& logicalDevice, const ConstZeroTerminatedStringView name);
	protected:
#if RENDERER_VULKAN
		VkImage m_pImage = 0;
#elif RENDERER_METAL || RENDERER_WEBGPU
		Internal::ImageData* m_pImage{nullptr};
#endif
	};
}
