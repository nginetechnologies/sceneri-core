#pragma once

#include <Renderer/Vulkan/ForwardDeclares.h>

#include <Common/Platform/ForceInline.h>
#include <Common/Platform/TrivialABI.h>

namespace ngine::Rendering
{
#if RENDERER_METAL || RENDERER_WEBGPU
	namespace Internal
	{
		struct FramebufferData;
	}
#endif

	struct TRIVIAL_ABI FramebufferView
	{
#if RENDERER_VULKAN
		[[nodiscard]] operator VkFramebuffer() const
		{
			return m_pFrameBuffer;
		}
#elif RENDERER_METAL || RENDERER_WEBGPU
		[[nodiscard]] operator Internal::FramebufferData*() const
		{
			return m_pFrameBuffer;
		}
#endif

		[[nodiscard]] bool IsValid() const
		{
#if RENDERER_VULKAN || RENDERER_METAL || RENDERER_WEBGPU
			return m_pFrameBuffer != 0;
#else
			return false;
#endif
		}
	protected:
#if RENDERER_VULKAN
		VkFramebuffer m_pFrameBuffer{0};
#elif RENDERER_METAL || RENDERER_WEBGPU
		Internal::FramebufferData* m_pFrameBuffer{nullptr};
#endif
	};
}
