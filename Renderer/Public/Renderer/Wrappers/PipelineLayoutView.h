#pragma once

#include <Renderer/Vulkan/ForwardDeclares.h>
#include <Renderer/WebGPU/ForwardDeclares.h>

#include <Common/Platform/ForceInline.h>
#include <Common/Platform/LifetimeBound.h>
#include <Common/Platform/TrivialABI.h>

namespace ngine::Rendering
{
#if RENDERER_METAL
	namespace Internal
	{
		struct PipelineLayoutData;
	}
#endif

	struct TRIVIAL_ABI PipelineLayoutView
	{
		PipelineLayoutView() = default;
#if RENDERER_VULKAN
		PipelineLayoutView(const VkPipelineLayout pPipelineLayout)
			: m_pPipelineLayout(pPipelineLayout)
		{
		}

		[[nodiscard]] operator VkPipelineLayout() const LIFETIME_BOUND
		{
			return m_pPipelineLayout;
		}
#elif RENDERER_METAL
		PipelineLayoutView(Internal::PipelineLayoutData* pPipelineLayout)
			: m_pPipelineLayout(pPipelineLayout)
		{
		}

		[[nodiscard]] operator Internal::PipelineLayoutData*() const LIFETIME_BOUND
		{
			return m_pPipelineLayout;
		}
#elif WEBGPU_INDIRECT_HANDLES
		PipelineLayoutView(WGPUPipelineLayout* pPipelineLayout)
			: m_pPipelineLayout(pPipelineLayout)
		{
		}

		[[nodiscard]] operator WGPUPipelineLayout() const LIFETIME_BOUND
		{
			return m_pPipelineLayout != nullptr ? *m_pPipelineLayout : nullptr;
		}
#elif RENDERER_WEBGPU
		PipelineLayoutView(const WGPUPipelineLayout pPipelineLayout)
			: m_pPipelineLayout(pPipelineLayout)
		{
		}

		[[nodiscard]] operator WGPUPipelineLayout() const LIFETIME_BOUND
		{
			return m_pPipelineLayout;
		}
#endif

		[[nodiscard]] bool IsValid() const
		{
#if RENDERER_VULKAN || RENDERER_METAL || RENDERER_WEBGPU
			return m_pPipelineLayout != 0;
#else
			return false;
#endif
		}
	protected:
#if RENDERER_VULKAN
		VkPipelineLayout m_pPipelineLayout = 0;
#elif RENDERER_METAL
		Internal::PipelineLayoutData* m_pPipelineLayout{nullptr};
#elif WEBGPU_INDIRECT_HANDLES
		WGPUPipelineLayout* m_pPipelineLayout = nullptr;
#elif RENDERER_WEBGPU
		WGPUPipelineLayout m_pPipelineLayout = nullptr;
#endif
	};
}
