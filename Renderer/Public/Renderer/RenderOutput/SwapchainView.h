#pragma once

#include <Renderer/Vulkan/ForwardDeclares.h>
#include <Renderer/Metal/ForwardDeclares.h>
#include <Renderer/WebGPU/ForwardDeclares.h>

#include <Common/Platform/ForceInline.h>
#include <Common/Platform/TrivialABI.h>

namespace ngine::Rendering
{
	struct SwapchainOutput;

	struct TRIVIAL_ABI SwapchainView
	{
		SwapchainView() = default;
#if RENDERER_VULKAN
		SwapchainView(VkSwapchainKHR pSwapchain)
			: m_pSwapchain(pSwapchain)
		{
		}
#elif RENDERER_METAL
		SwapchainView(CAMetalLayer* pSwapchain)
			: m_pSwapchain(pSwapchain)
		{
		}
#elif RENDERER_WEBGPU_WGPU_NATIVE || RENDERER_WEBGPU_DAWN
		SwapchainView(WGPUSurface pSwapchain)
			: m_pSwapchain(pSwapchain)
		{
		}
#elif RENDERER_WEBGPU
		SwapchainView(WGPUSwapChain pSwapchain)
			: m_pSwapchain(pSwapchain)
		{
		}
#endif

#if RENDERER_VULKAN
		[[nodiscard]] operator VkSwapchainKHR() const
		{
			return m_pSwapchain;
		}
#elif RENDERER_METAL
		[[nodiscard]] operator CAMetalLayer*() const
		{
			return m_pSwapchain;
		}
		[[nodiscard]] operator id<CAMetalDrawable>() const
		{
			return m_currentDrawable;
		}
#elif RENDERER_WEBGPU_WGPU_NATIVE || RENDERER_WEBGPU_DAWN
		[[nodiscard]] operator WGPUSurface() const
		{
			return m_pSwapchain;
		}
#elif RENDERER_WEBGPU
		[[nodiscard]] operator WGPUSwapChain() const
		{
			return m_pSwapchain;
		}
#endif

		[[nodiscard]] bool IsValid() const
		{
#if RENDERER_VULKAN || RENDERER_METAL || RENDERER_WEBGPU
			return m_pSwapchain != 0;
#else
			return false;
#endif
		}
	protected:
		friend SwapchainOutput;

#if RENDERER_VULKAN
		VkSwapchainKHR m_pSwapchain = 0;
#elif RENDERER_METAL
		CAMetalLayer* m_pSwapchain{nullptr};
		id<CAMetalDrawable> m_currentDrawable{nullptr};
#elif RENDERER_WEBGPU_WGPU_NATIVE || RENDERER_WEBGPU_DAWN
		WGPUSurface m_pSwapchain = nullptr;
#elif RENDERER_WEBGPU
		WGPUSwapChain m_pSwapchain = nullptr;
#endif
	};
}
