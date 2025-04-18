#pragma once

#include <Renderer/Vulkan/ForwardDeclares.h>
#include <Renderer/Metal/ForwardDeclares.h>
#include <Renderer/WebGPU/ForwardDeclares.h>

#include <Common/Platform/ForceInline.h>
#include <Common/Platform/TrivialABI.h>
#include <Common/Memory/Optional.h>

namespace ngine::Rendering
{
	struct Renderer;
	struct PhysicalDevice;
	struct LogicalDevice;

	struct TRIVIAL_ABI SurfaceView
	{
		SurfaceView() = default;

#if RENDERER_VULKAN
		SurfaceView(VkSurfaceKHR pSurface)
			: m_pSurface(pSurface)
		{
		}

		[[nodiscard]] operator VkSurfaceKHR() const
		{
			return m_pSurface;
		}
#elif RENDERER_WEBGPU
		SurfaceView(WGPUSurface pSurface)
			: m_pSurface(pSurface)
		{
		}
		[[nodiscard]] operator WGPUSurface() const
		{
			return m_pSurface;
		}
#elif RENDERER_METAL
		SurfaceView(CAMetalLayer* pLayer)
			: m_pSurface(pLayer)
		{
		}
		[[nodiscard]] operator CAMetalLayer*() const
		{
			return m_pSurface;
		}
#endif

		[[nodiscard]] bool IsValid() const
		{
#if RENDERER_VULKAN || RENDERER_METAL || RENDERER_WEBGPU
			return m_pSurface != nullptr;
#else
			return false;
#endif
		}

#if RENDERER_VULKAN
		[[nodiscard]] Optional<const PhysicalDevice*>
		GetBestPhysicalDevice(const Renderer& renderer, const Optional<uint8*> pPresentQueueIndexOut = Invalid) const;
#else
		[[nodiscard]] static Optional<const PhysicalDevice*>
		GetBestPhysicalDevice(const Renderer& renderer, const Optional<uint8*> pPresentQueueIndexOut = Invalid);
#endif
		[[nodiscard]] Optional<LogicalDevice*> CreateLogicalDeviceForSurface(Renderer& renderer) const;
	protected:
#if RENDERER_VULKAN
		VkSurfaceKHR m_pSurface = nullptr;
#elif RENDERER_METAL
		CAMetalLayer* m_pSurface{nullptr};
#elif RENDERER_WEBGPU
		WGPUSurface m_pSurface = nullptr;
#endif
	};
}
