#pragma once

#include "EncodedParallelCommandBufferView.h"

namespace ngine::Rendering
{
	struct ParallelRenderCommandEncoder;

	struct EncodedParallelCommandBuffer : public EncodedParallelCommandBufferView
	{
		EncodedParallelCommandBuffer() = default;
		EncodedParallelCommandBuffer(const EncodedParallelCommandBuffer&) = delete;
		EncodedParallelCommandBuffer& operator=(const EncodedParallelCommandBuffer&) = delete;
		EncodedParallelCommandBuffer([[maybe_unused]] EncodedParallelCommandBuffer&& other) noexcept
		{
#if RENDERER_VULKAN || RENDERER_METAL || RENDERER_WEBGPU
			m_pCommandBuffer = other.m_pCommandBuffer;
			other.m_pCommandBuffer = nullptr;
#endif
		}
		EncodedParallelCommandBuffer& operator=(EncodedParallelCommandBuffer&& other) noexcept;
		~EncodedParallelCommandBuffer();
	protected:
		EncodedParallelCommandBuffer(const EncodedParallelCommandBufferView commandEncoder)
			: EncodedParallelCommandBufferView(commandEncoder)
		{
		}

		friend ParallelRenderCommandEncoder;
	};
}
