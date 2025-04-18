#pragma once

#include <Renderer/Vulkan/ForwardDeclares.h>
#include <Renderer/WebGPU/ForwardDeclares.h>

#include <Common/Platform/TrivialABI.h>

namespace ngine::Rendering
{
	struct TRIVIAL_ABI InstanceView
	{
#if RENDERER_VULKAN
		InstanceView() = default;
		InstanceView(VkInstance pInstance)
			: m_pInstance(pInstance)
		{
		}
#elif RENDERER_WEBGPU
		InstanceView() = default;
		InstanceView(WGPUInstance pInstance)
			: m_pInstance(pInstance)
		{
		}
#else
		InstanceView() = default;
#endif

#if RENDERER_VULKAN
		[[nodiscard]] operator VkInstance() const
		{
			return m_pInstance;
		}
#elif RENDERER_WEBGPU
		[[nodiscard]] operator WGPUInstance() const
		{
			return m_pInstance;
		}
#endif

		[[nodiscard]] bool IsValid() const
		{
#if RENDERER_VULKAN || RENDERER_WEBGPU
			return m_pInstance != nullptr;
#else
			return true;
#endif
		}
	protected:
#if RENDERER_VULKAN
		VkInstance m_pInstance{nullptr};
		VkDebugUtilsMessengerEXT m_pDebugMessenger{nullptr};
#elif RENDERER_WEBGPU
		WGPUInstance m_pInstance{nullptr};
#endif
	};
}
