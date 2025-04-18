#pragma once

#include <Renderer/Vulkan/ForwardDeclares.h>
#include <Renderer/Metal/ForwardDeclares.h>
#include <Renderer/WebGPU/ForwardDeclares.h>

#include <Common/Platform/ForceInline.h>
#include <Common/Platform/LifetimeBound.h>
#include <Common/Platform/TrivialABI.h>

namespace ngine::Rendering
{
	struct LogicalDevice;

	struct TRIVIAL_ABI GraphicsPipelineView
	{
		GraphicsPipelineView() = default;
#if RENDERER_VULKAN
		GraphicsPipelineView(const VkPipeline pipeline)
			: m_pPipeline(pipeline)
		{
		}

		[[nodiscard]] operator VkPipeline() const LIFETIME_BOUND
		{
			return m_pPipeline;
		}
#elif RENDERER_METAL
		GraphicsPipelineView(const id<MTLRenderPipelineState> pipeline)
			: m_pPipeline(pipeline)
		{
		}

		[[nodiscard]] operator id<MTLRenderPipelineState>() const LIFETIME_BOUND
		{
			return m_pPipeline;
		}
#elif RENDERER_WEBGPU
		GraphicsPipelineView(const WGPURenderPipeline pipeline)
			: m_pPipeline(pipeline)
		{
		}

		[[nodiscard]] operator WGPURenderPipeline() const LIFETIME_BOUND
		{
			return m_pPipeline;
		}
#endif

		[[nodiscard]] bool IsValid() const
		{
#if RENDERER_VULKAN || RENDERER_METAL || RENDERER_WEBGPU
			return m_pPipeline != 0;
#else
			return false;
#endif
		}

		void SetDebugName(const LogicalDevice& logicalDevice, const ConstZeroTerminatedStringView name);
	protected:
#if RENDERER_VULKAN
		VkPipeline m_pPipeline = 0;
#elif RENDERER_METAL
		id<MTLRenderPipelineState> m_pPipeline = nullptr;
#elif RENDERER_WEBGPU
		WGPURenderPipeline m_pPipeline = nullptr;
#endif
	};
}
