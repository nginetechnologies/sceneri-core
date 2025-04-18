#pragma once

#include <Common/Platform/ForceInline.h>
#include <Common/Platform/TrivialABI.h>
#include <Common/EnumFlags.h>

#include <Renderer/Vulkan/ForwardDeclares.h>
#include <Renderer/Metal/ForwardDeclares.h>
#include <Renderer/WebGPU/ForwardDeclares.h>

namespace ngine::Rendering
{
	struct TRIVIAL_ABI EncodedCommandBufferView
	{
		EncodedCommandBufferView() = default;
#if RENDERER_VULKAN
		constexpr EncodedCommandBufferView(VkCommandBuffer pCommandBuffer)
			: m_pCommandBuffer(pCommandBuffer)
		{
		}

		[[nodiscard]] operator VkCommandBuffer() const
		{
			return m_pCommandBuffer;
		}
#elif RENDERER_METAL
		constexpr EncodedCommandBufferView(id<MTLCommandBuffer> pCommandBuffer)
			: m_pCommandBuffer(pCommandBuffer)
		{
		}

		[[nodiscard]] operator id<MTLCommandBuffer>() const
		{
			return m_pCommandBuffer;
		}
#elif WEBGPU_INDIRECT_HANDLES
		constexpr EncodedCommandBufferView(WGPUCommandBuffer* pCommandBuffer)
			: m_pCommandBuffer(pCommandBuffer)
		{
		}

		[[nodiscard]] operator WGPUCommandBuffer() const
		{
			return m_pCommandBuffer != nullptr ? *m_pCommandBuffer : nullptr;
		}
#elif RENDERER_WEBGPU
		constexpr EncodedCommandBufferView(WGPUCommandBuffer pCommandBuffer)
			: m_pCommandBuffer(pCommandBuffer)
		{
		}

		[[nodiscard]] operator WGPUCommandBuffer() const
		{
			return m_pCommandBuffer;
		}
#endif

		[[nodiscard]] bool IsValid() const
		{
#if RENDERER_VULKAN || RENDERER_WEBGPU || RENDERER_METAL
			return m_pCommandBuffer != nullptr;
#else
			return false;
#endif
		}
	protected:
		friend struct EncodedCommandBuffer;

#if RENDERER_VULKAN
		VkCommandBuffer m_pCommandBuffer = nullptr;
#elif RENDERER_METAL
		id<MTLCommandBuffer> m_pCommandBuffer;
#elif WEBGPU_INDIRECT_HANDLES
		WGPUCommandBuffer* m_pCommandBuffer = nullptr;
#elif RENDERER_WEBGPU
		WGPUCommandBuffer m_pCommandBuffer = nullptr;
#endif
	};
}
