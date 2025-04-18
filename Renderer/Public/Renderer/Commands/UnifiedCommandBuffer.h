#pragma once

#include "CommandBuffer.h"
#include "CommandEncoder.h"
#include "EncodedCommandBuffer.h"

namespace ngine::Rendering
{
	struct UnifiedCommandBuffer : public CommandBuffer
	{
		UnifiedCommandBuffer() = default;
		UnifiedCommandBuffer(const LogicalDeviceView logicalDevice, const CommandPoolView commandPool, const CommandQueueView commandQueue);
		UnifiedCommandBuffer(const UnifiedCommandBuffer&) = delete;
		UnifiedCommandBuffer& operator=(const UnifiedCommandBuffer&) = delete;
		UnifiedCommandBuffer(UnifiedCommandBuffer&& other) = default;
		UnifiedCommandBuffer& operator=(UnifiedCommandBuffer&& other) = default;

		void Destroy(const LogicalDeviceView logicalDevice, const CommandPoolView commandPool);

		[[nodiscard]] operator CommandEncoderView() const
		{
			return m_commandEncoder;
		}
		[[nodiscard]] operator EncodedCommandBufferView() const
		{
			Assert(!m_commandEncoder.IsValid());
			return m_encodedCommandBuffer;
		}

		[[nodiscard]] bool IsValid() const
		{
			return CommandBuffer::IsValid() | m_commandEncoder.IsValid();
		}
		[[nodiscard]] bool IsEncoding() const
		{
			return m_commandEncoder.IsValid();
		}
		[[nodiscard]] bool IsEncoded() const
		{
			return m_encodedCommandBuffer.IsValid();
		}

		CommandEncoderView
		BeginEncoding(const Rendering::LogicalDeviceView logicalDevice, const CommandBuffer::Flags flags = CommandBuffer::Flags());
		EncodedCommandBufferView StopEncoding()
		{
			Assert(m_commandEncoder.IsValid());
			Assert(!m_encodedCommandBuffer.IsValid());
			m_encodedCommandBuffer = m_commandEncoder.StopEncoding();
			m_commandEncoder = {};
			return m_encodedCommandBuffer;
		}
	protected:
		// #if RENDERER_WEBGPU
		CommandEncoder m_commandEncoder;
		EncodedCommandBuffer m_encodedCommandBuffer;
		// #endif
	};
}
