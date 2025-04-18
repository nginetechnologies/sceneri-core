#pragma once

#include <Renderer/Vulkan/ForwardDeclares.h>

#include <Common/Platform/ForceInline.h>
#include <Common/Platform/TrivialABI.h>

namespace ngine::Rendering
{
	namespace Internal
	{
#if RENDERER_METAL || RENDERER_WEBGPU
		struct RenderPassData;
#endif
	}

	struct TRIVIAL_ABI RenderPassView
	{
		RenderPassView() = default;
#if RENDERER_VULKAN
		RenderPassView(const VkRenderPass pRenderPass)
			: m_pRenderPass(pRenderPass)
		{
		}

		[[nodiscard]] operator VkRenderPass() const
		{
			return m_pRenderPass;
		}

#elif RENDERER_METAL || RENDERER_WEBGPU
		RenderPassView(Internal::RenderPassData* const pRenderPass)
			: m_pRenderPass(pRenderPass)
		{
		}

		[[nodiscard]] operator Internal::RenderPassData*() const
		{
			return m_pRenderPass;
		}
#endif

		RenderPassView(const RenderPassView&) = default;
		RenderPassView& operator=(const RenderPassView&) = default;
		RenderPassView(RenderPassView&&) = default;
		RenderPassView& operator=(RenderPassView&&) = default;
		~RenderPassView() = default;

		[[nodiscard]] bool IsValid() const
		{
#if RENDERER_VULKAN || RENDERER_METAL || RENDERER_WEBGPU
			return m_pRenderPass != 0;
#else
			return false;
#endif
		}
	protected:
#if RENDERER_VULKAN
		VkRenderPass m_pRenderPass{0};
#elif RENDERER_METAL || RENDERER_WEBGPU
		Internal::RenderPassData* m_pRenderPass{nullptr};
#endif
	};
}
