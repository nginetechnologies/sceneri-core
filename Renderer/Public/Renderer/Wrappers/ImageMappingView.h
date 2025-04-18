#pragma once

#include <Renderer/Vulkan/ForwardDeclares.h>
#include <Renderer/Metal/ForwardDeclares.h>
#include <Renderer/WebGPU/ForwardDeclares.h>

#include <Common/Platform/ForceInline.h>
#include <Common/Platform/LifetimeBound.h>
#include <Common/Platform/TrivialABI.h>

namespace ngine::Rendering
{
#if RENDERER_METAL || RENDERER_WEBGPU
	namespace Internal
	{
		struct ImageMappingData;
	}
#endif

	struct TRIVIAL_ABI ImageMappingView
	{
		ImageMappingView() = default;
#if RENDERER_VULKAN
		ImageMappingView(const VkImageView pImageView)
			: m_pImageView(pImageView)
		{
		}

		[[nodiscard]] operator VkImageView() const
		{
			return m_pImageView;
		}
#elif RENDERER_METAL || RENDERER_WEBGPU
		ImageMappingView(Internal::ImageMappingData* pImageView)
			: m_pImageView(pImageView)
		{
		}

#if RENDERER_METAL
		[[nodiscard]] operator id<MTLTexture>() const;
#elif RENDERER_WEBGPU
		[[nodiscard]] operator WGPUTextureView() const;
#endif

		[[nodiscard]] operator Internal::ImageMappingData*() const
		{
			return m_pImageView;
		}
#endif

		[[nodiscard]] bool IsValid() const
		{
#if RENDERER_VULKAN || RENDERER_METAL || RENDERER_WEBGPU
			return m_pImageView != 0;
#else
			return false;
#endif
		}

		[[nodiscard]] bool operator==(const ImageMappingView other) const
		{
			return m_pImageView == other.m_pImageView;
		}
	protected:
#if RENDERER_VULKAN
		VkImageView m_pImageView = 0;
#elif RENDERER_METAL || RENDERER_WEBGPU
		Internal::ImageMappingData* m_pImageView{nullptr};
#endif
	};
}
