#pragma once

#include "EncodedCommandBufferView.h"

namespace ngine::Rendering
{
	struct CommandEncoder;
	struct ParallelRenderCommandEncoder;

	struct EncodedCommandBuffer : public EncodedCommandBufferView
	{
		EncodedCommandBuffer() = default;
		EncodedCommandBuffer(const EncodedCommandBuffer&) = delete;
		EncodedCommandBuffer& operator=(const EncodedCommandBuffer&) = delete;
		EncodedCommandBuffer([[maybe_unused]] EncodedCommandBuffer&& other) noexcept
		{
#if RENDERER_VULKAN || RENDERER_WEBGPU || RENDERER_METAL
			m_pCommandBuffer = other.m_pCommandBuffer;
			other.m_pCommandBuffer = nullptr;
#endif
		}
		EncodedCommandBuffer& operator=(EncodedCommandBuffer&& other) noexcept;
		~EncodedCommandBuffer();
	protected:
		EncodedCommandBuffer(const EncodedCommandBufferView commandEncoder)
			: EncodedCommandBufferView(commandEncoder)
		{
		}

		friend CommandEncoder;
		friend ParallelRenderCommandEncoder;
	};
}
