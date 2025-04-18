#pragma once

#include "ComputeCommandEncoderView.h"

#include <Renderer/Wrappers/FramebufferView.h>
#include <Common/Math/Primitives/Rectangle.h>

namespace ngine::Rendering
{
	struct CommandEncoder;
	struct CommandEncoderView;

	struct ComputeCommandEncoder : public ComputeCommandEncoderView
	{
		ComputeCommandEncoder() = default;
		ComputeCommandEncoder(const ComputeCommandEncoder&) = delete;
		ComputeCommandEncoder& operator=(const ComputeCommandEncoder&) = delete;
		ComputeCommandEncoder([[maybe_unused]] ComputeCommandEncoder&& other) noexcept
		{
#if RENDERER_VULKAN || RENDERER_WEBGPU || RENDERER_METAL
			m_pCommandEncoder = other.m_pCommandEncoder;
			other.m_pCommandEncoder = nullptr;
#endif
		}
		ComputeCommandEncoder& operator=(ComputeCommandEncoder&& other) noexcept;
		~ComputeCommandEncoder();

		void End();
	protected:
		ComputeCommandEncoder(const ComputeCommandEncoderView computeCommandEncoder)
			: ComputeCommandEncoderView(computeCommandEncoder)
		{
		}
		friend CommandEncoder;
		friend CommandEncoderView;
	};
}
