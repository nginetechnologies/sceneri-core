#pragma once

#include <Common/Platform/ForceInline.h>
#include <Common/Platform/TrivialABI.h>

#include <Renderer/Vulkan/ForwardDeclares.h>
#include <Renderer/Metal/ForwardDeclares.h>
#include <Renderer/WebGPU/ForwardDeclares.h>

namespace ngine::Rendering
{
	struct CommandQueueView;

	struct TRIVIAL_ABI LogicalDeviceView
	{
		LogicalDeviceView() = default;

#if RENDERER_VULKAN
		LogicalDeviceView(VkDevice pDevice)
			: m_pDevice(pDevice)
		{
		}

		[[nodiscard]] operator VkDevice() const
		{
			return m_pDevice;
		}
#elif RENDERER_METAL
		LogicalDeviceView(id<MTLDevice> device)
			: m_pDevice(device)
		{
		}

		[[nodiscard]] operator id<MTLDevice>() const
		{
			return m_pDevice;
		}
#elif RENDERER_WEBGPU
		LogicalDeviceView(WGPUDevice pDevice)
			: m_pDevice(pDevice)
		{
		}
		[[nodiscard]] operator WGPUDevice() const
		{
			return m_pDevice;
		}
#endif

		[[nodiscard]] bool IsValid() const
		{
#if RENDERER_VULKAN || RENDERER_WEBGPU || RENDERER_METAL
			return m_pDevice != nullptr;
#else
			return false;
#endif
		}
	protected:
#if RENDERER_VULKAN
		VkDevice m_pDevice = nullptr;
#elif RENDERER_METAL
		id<MTLDevice> m_pDevice;
#elif RENDERER_WEBGPU
		WGPUDevice m_pDevice = nullptr;
#endif
	};
}
