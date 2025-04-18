#pragma once

#include "BlitCommandEncoderView.h"

namespace ngine::Rendering
{
	struct CommandEncoder;
	struct CommandEncoderView;

	struct BlitCommandEncoder : public BlitCommandEncoderView
	{
		BlitCommandEncoder() = default;
		BlitCommandEncoder(const BlitCommandEncoder&) = delete;
		BlitCommandEncoder& operator=(const BlitCommandEncoder&) = delete;
		BlitCommandEncoder([[maybe_unused]] BlitCommandEncoder&& other) noexcept
		{
#if RENDERER_VULKAN || RENDERER_WEBGPU || RENDERER_METAL
			m_pCommandEncoder = other.m_pCommandEncoder;
			other.m_pCommandEncoder = nullptr;
#endif
		}
		BlitCommandEncoder& operator=(BlitCommandEncoder&& other) noexcept;
		~BlitCommandEncoder();

		void End();
	protected:
		BlitCommandEncoder(const BlitCommandEncoderView computeCommandEncoder)
			: BlitCommandEncoderView(computeCommandEncoder)
		{
		}
		friend CommandEncoder;
		friend CommandEncoderView;
	};
}
