#pragma once

#include <Common/Platform/ForceInline.h>
#include <Common/Platform/TrivialABI.h>
#include <Common/EnumFlags.h>

#include <Renderer/Vulkan/ForwardDeclares.h>
#include <Renderer/Metal/ForwardDeclares.h>
#include <Renderer/WebGPU/ForwardDeclares.h>

namespace ngine::Rendering
{
	struct TRIVIAL_ABI EncodedParallelCommandBufferView
	{
		constexpr EncodedParallelCommandBufferView() = default;
#if RENDERER_VULKAN
		constexpr EncodedParallelCommandBufferView(VkCommandBuffer pCommandBuffer)
			: m_pCommandBuffer(pCommandBuffer)
		{
		}
		[[nodiscard]] operator VkCommandBuffer() const
		{
			return m_pCommandBuffer;
		}
#elif RENDERER_METAL
		constexpr EncodedParallelCommandBufferView(id<MTLParallelRenderCommandEncoder> pCommandBuffer)
			: m_pCommandBuffer(pCommandBuffer)
		{
		}
		[[nodiscard]] operator id<MTLParallelRenderCommandEncoder>() const
		{
			return m_pCommandBuffer;
		}
#elif WEBGPU_INDIRECT_HANDLES
		constexpr EncodedParallelCommandBufferView(WGPURenderBundle* pCommandBuffer)
			: m_pCommandBuffer(pCommandBuffer)
		{
		}
		[[nodiscard]] operator WGPURenderBundle() const
		{
			return m_pCommandBuffer != nullptr ? *m_pCommandBuffer : nullptr;
		}
#elif RENDERER_WEBGPU
		constexpr EncodedParallelCommandBufferView(WGPURenderBundle pCommandBuffer)
			: m_pCommandBuffer(pCommandBuffer)
		{
		}
		[[nodiscard]] operator WGPURenderBundle() const
		{
			return m_pCommandBuffer;
		}
#endif

		[[nodiscard]] bool IsValid() const
		{
#if RENDERER_VULKAN || RENDERER_METAL || RENDERER_WEBGPU
			return m_pCommandBuffer != nullptr;
#else
			return false;
#endif
		}
	protected:
		friend struct EncodedParallelCommandBuffer;

#if RENDERER_VULKAN
		VkCommandBuffer m_pCommandBuffer = nullptr;
#elif RENDERER_METAL
		id<MTLParallelRenderCommandEncoder> m_pCommandBuffer{nullptr};
#elif WEBGPU_INDIRECT_HANDLES
		WGPURenderBundle* m_pCommandBuffer = nullptr;
#elif RENDERER_WEBGPU
		WGPURenderBundle m_pCommandBuffer = nullptr;
#endif
	};
}
