#pragma once

#include "CommandEncoderView.h"

namespace ngine::Rendering
{
	struct CommandBuffer;
	struct CommandBufferView;

	struct EncodedCommandBuffer;

	struct CommandEncoder : public CommandEncoderView
	{
		CommandEncoder() = default;
		CommandEncoder(const CommandEncoder&) = delete;
		CommandEncoder& operator=(const CommandEncoder&) = delete;
		CommandEncoder([[maybe_unused]] CommandEncoder&& other) noexcept
		{
#if RENDERER_VULKAN || RENDERER_WEBGPU || RENDERER_METAL
			m_pCommandEncoder = other.m_pCommandEncoder;
			other.m_pCommandEncoder = nullptr;
#endif
		}
		CommandEncoder& operator=(CommandEncoder&& other) noexcept;
		~CommandEncoder();

		[[nodiscard]] EncodedCommandBuffer StopEncoding();
	protected:
		CommandEncoder(const CommandEncoderView commandEncoder)
			: CommandEncoderView(commandEncoder)
		{
		}
		friend CommandBuffer;
		friend CommandBufferView;
	};
}
