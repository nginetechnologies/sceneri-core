#pragma once

#include <Renderer/Vulkan/ForwardDeclares.h>
#include <Renderer/Metal/ForwardDeclares.h>
#include <Renderer/WebGPU/ForwardDeclares.h>

#include <Common/Platform/ForceInline.h>
#include <Common/Platform/TrivialABI.h>

namespace ngine::Rendering
{
	struct TRIVIAL_ABI SamplerView
	{
		SamplerView() = default;
#if RENDERER_VULKAN
		SamplerView(const VkSampler sampler)
			: m_pSampler(sampler)
		{
		}
		[[nodiscard]] operator VkSampler() const
		{
			return m_pSampler;
		}
#elif RENDERER_METAL
		SamplerView(id<MTLSamplerState> sampler)
			: m_pSampler(sampler)
		{
		}
		[[nodiscard]] operator id<MTLSamplerState>() const
		{
			return m_pSampler;
		}
#elif RENDERER_WEBGPU
		SamplerView(const WGPUSampler sampler)
			: m_pSampler(sampler)
		{
		}
		[[nodiscard]] operator WGPUSampler() const
		{
			return m_pSampler;
		}
#endif

		[[nodiscard]] bool IsValid() const
		{
#if RENDERER_VULKAN || RENDERER_WEBGPU || RENDERER_METAL
			return m_pSampler != 0;
#else
			return false;
#endif
		}
	protected:
#if RENDERER_VULKAN
		VkSampler m_pSampler = 0;
#elif RENDERER_METAL
		id<MTLSamplerState> m_pSampler;
#elif RENDERER_WEBGPU
		WGPUSampler m_pSampler = nullptr;
#endif
	};
}
