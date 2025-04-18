#pragma once

#include "RenderCommandEncoderView.h"
#include "CommandEncoderView.h"

#include <Renderer/Wrappers/FramebufferView.h>
#include <Renderer/Wrappers/RenderPassView.h>
#include <Common/Math/Primitives/Rectangle.h>

namespace ngine::Rendering
{
	struct CommandEncoder;
	struct CommandEncoderView;

	struct RenderCommandEncoder : public RenderCommandEncoderView
	{
		RenderCommandEncoder() = default;
		RenderCommandEncoder(const RenderCommandEncoder&) = delete;
		RenderCommandEncoder& operator=(const RenderCommandEncoder&) = delete;
		RenderCommandEncoder([[maybe_unused]] RenderCommandEncoder&& other) noexcept
#if RENDERER_METAL || RENDERER_WEBGPU
			: m_commandEncoder(other.m_commandEncoder)
			, m_renderPass(other.m_renderPass)
			, m_framebuffer(other.m_framebuffer)
			, m_currentSubpassIndex(other.m_currentSubpassIndex)
#endif
		{
#if RENDERER_VULKAN || RENDERER_WEBGPU || RENDERER_METAL
			m_pCommandEncoder = other.m_pCommandEncoder;
			other.m_pCommandEncoder = nullptr;
#endif
		}
		RenderCommandEncoder& operator=(RenderCommandEncoder&& other) noexcept;
		~RenderCommandEncoder();

		void End();
		void StartNextSubpass(const ArrayView<const ClearValue, uint8> clearValues);
	protected:
#if RENDERER_METAL || RENDERER_WEBGPU
		RenderCommandEncoder(
			const RenderCommandEncoderView renderCommandEncoder,
			const CommandEncoderView commandEncoder,
			const RenderPassView renderPass,
			const FramebufferView framebuffer,
			const uint8 currentSubpassIndex
		)
			: RenderCommandEncoderView(renderCommandEncoder)
			, m_commandEncoder(commandEncoder)
			, m_renderPass(renderPass)
			, m_framebuffer(framebuffer)
			, m_currentSubpassIndex(currentSubpassIndex)
		{
		}
#else
		RenderCommandEncoder(RenderCommandEncoderView&& commandEncoder)
			: RenderCommandEncoderView(Forward<RenderCommandEncoderView>(commandEncoder))
		{
		}
		RenderCommandEncoder(RenderCommandEncoder& renderCommandEncoder)
			: RenderCommandEncoderView(renderCommandEncoder)
		{
		}
#endif
		friend CommandEncoder;
		friend CommandEncoderView;

#if RENDERER_METAL || RENDERER_WEBGPU
		CommandEncoderView m_commandEncoder;
		RenderPassView m_renderPass;
		FramebufferView m_framebuffer;
		uint8 m_currentSubpassIndex{0};
#endif
	};
}
