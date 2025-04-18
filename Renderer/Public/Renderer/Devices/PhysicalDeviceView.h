#pragma once

#include <Renderer/Vulkan/ForwardDeclares.h>
#include <Renderer/Metal/ForwardDeclares.h>
#include <Renderer/WebGPU/ForwardDeclares.h>

#include <Common/Platform/TrivialABI.h>

namespace ngine::Rendering
{
	struct TRIVIAL_ABI PhysicalDeviceView
	{
		PhysicalDeviceView() = default;

#if RENDERER_VULKAN
		PhysicalDeviceView(VkPhysicalDevice pDevice)
			: m_pDevice(pDevice)
		{
		}

		[[nodiscard]] operator VkPhysicalDevice() const
		{
			return m_pDevice;
		}
#elif RENDERER_METAL
		PhysicalDeviceView(id<MTLDevice> device)
			: m_pDevice(device)
		{
		}

		[[nodiscard]] operator id<MTLDevice>() const
		{
			return m_pDevice;
		}
#elif RENDERER_WEBGPU
		PhysicalDeviceView(WGPUAdapter pDevice)
			: m_pDevice(pDevice)
		{
		}

		[[nodiscard]] operator WGPUAdapter() const
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

		[[nodiscard]] bool operator==([[maybe_unused]] const PhysicalDeviceView& other) const
		{
#if RENDERER_VULKAN || RENDERER_WEBGPU || RENDERER_METAL
			return m_pDevice == other.m_pDevice;
#else
			return false;
#endif
		}
		[[nodiscard]] bool operator!=(const PhysicalDeviceView& other) const
		{
			return !operator==(other);
		}
	protected:
#if RENDERER_VULKAN
		VkPhysicalDevice m_pDevice = nullptr;
#elif RENDERER_METAL
		id<MTLDevice> m_pDevice;
#elif RENDERER_WEBGPU
		WGPUAdapter m_pDevice = nullptr;
#endif
	};
}
