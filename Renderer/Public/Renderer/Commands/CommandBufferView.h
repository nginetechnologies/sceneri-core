#pragma once

#include <Common/Platform/ForceInline.h>
#include <Common/Platform/TrivialABI.h>
#include <Common/EnumFlags.h>

#include <Renderer/Vulkan/ForwardDeclares.h>
#include <Renderer/Metal/ForwardDeclares.h>

namespace ngine::Rendering
{
	struct CommandEncoder;
	struct LogicalDeviceView;

	struct TRIVIAL_ABI CommandBufferView
	{
		enum class Flags : uint8
		{
			OneTimeSubmit = 1 << 0
		};

		CommandBufferView() = default;
#if RENDERER_VULKAN
		constexpr CommandBufferView(VkCommandBuffer pCommandBuffer)
			: m_pCommandBuffer(pCommandBuffer)
		{
		}

		[[nodiscard]] operator VkCommandBuffer() const
		{
			return m_pCommandBuffer;
		}
#elif RENDERER_METAL
		constexpr CommandBufferView(id<MTLCommandBuffer> commandBuffer)
			: m_pCommandBuffer(commandBuffer)
		{
		}

		[[nodiscard]] operator id<MTLCommandBuffer>() const
		{
			return m_pCommandBuffer;
		}
#elif RENDERER_WEBGPU
		constexpr explicit CommandBufferView(const bool isValid)
			: m_isValid(isValid)
		{
		}
#endif

		[[nodiscard]] bool IsValid() const
		{
#if RENDERER_VULKAN || RENDERER_METAL
			return m_pCommandBuffer != nullptr;
#elif RENDERER_WEBGPU
			return m_isValid;
#else
			return false;
#endif
		}

		[[nodiscard]] CommandEncoder BeginEncoding(const LogicalDeviceView logicalDevice, const Flags flags = Flags()) const;
	protected:
		friend struct CommandBuffer;

#if RENDERER_VULKAN
		VkCommandBuffer m_pCommandBuffer = nullptr;
#elif RENDERER_METAL
		id<MTLCommandBuffer> m_pCommandBuffer;
#elif RENDERER_WEBGPU
		bool m_isValid{false};
#endif
	};
}
