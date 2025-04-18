#pragma once

#include "CommandBufferView.h"

namespace ngine::Rendering
{
	struct LogicalDeviceView;
	struct CommandPoolView;
	struct CommandQueueView;
	struct JobRunnerData;

	struct CommandBuffer : public CommandBufferView
	{
		CommandBuffer() = default;
		CommandBuffer(const LogicalDeviceView logicalDevice, const CommandPoolView commandPool, const CommandQueueView commandQueue);
		CommandBuffer(const CommandBuffer&) = delete;
		CommandBuffer& operator=(const CommandBuffer&) = delete;
		CommandBuffer([[maybe_unused]] CommandBuffer&& other) noexcept
		{
#if RENDERER_VULKAN || RENDERER_METAL
			m_pCommandBuffer = other.m_pCommandBuffer;
			other.m_pCommandBuffer = nullptr;
#elif RENDERER_WEBGPU
			m_isValid = other.m_isValid;
			other.m_isValid = false;
#endif
		}
		CommandBuffer& operator=(CommandBuffer&& other) noexcept;
		~CommandBuffer();

		void Destroy(const LogicalDeviceView logicalDevice, const CommandPoolView commandPool);
	protected:
		friend CommandPoolView;
		friend JobRunnerData;
		void OnBufferFreed()
		{
#if RENDERER_VULKAN || RENDERER_METAL
			m_pCommandBuffer = nullptr;
#elif RENDERER_WEBGPU
			m_isValid = false;
#endif
		}
	};
}
