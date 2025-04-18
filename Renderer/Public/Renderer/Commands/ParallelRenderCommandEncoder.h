#pragma once

#include "ParallelRenderCommandEncoderView.h"

#include <Renderer/Wrappers/FramebufferView.h>
#include <Common/Math/Primitives/Rectangle.h>

namespace ngine::Rendering
{
	struct CommandEncoder;
	struct CommandEncoderView;

	struct EncodedParallelCommandBuffer;

	struct ParallelRenderCommandEncoder : public ParallelRenderCommandEncoderView
	{
		ParallelRenderCommandEncoder() = default;
		ParallelRenderCommandEncoder(const ParallelRenderCommandEncoder&) = delete;
		ParallelRenderCommandEncoder& operator=(const ParallelRenderCommandEncoder&) = delete;
		ParallelRenderCommandEncoder([[maybe_unused]] ParallelRenderCommandEncoder&& other) noexcept
		{
#if RENDERER_VULKAN || RENDERER_WEBGPU || RENDERER_METAL
			m_pCommandEncoder = other.m_pCommandEncoder;
			other.m_pCommandEncoder = nullptr;
#endif
		}
		ParallelRenderCommandEncoder& operator=(ParallelRenderCommandEncoder&& other) noexcept;
		~ParallelRenderCommandEncoder();

		[[nodiscard]] EncodedParallelCommandBuffer StopEncoding();
	protected:
		ParallelRenderCommandEncoder(const ParallelRenderCommandEncoderView renderCommandEncoder)
			: ParallelRenderCommandEncoderView(renderCommandEncoder)
		{
		}
		friend CommandEncoder;
		friend CommandEncoderView;
	};
}
