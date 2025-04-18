#pragma once

#include <Renderer/Commands/CommandPoolView.h>
#include <Renderer/Commands/UnifiedCommandBuffer.h>
#include <Renderer/Devices/QueueFamily.h>

#include <Common/Memory/Move.h>
#include <Common/Function/Function.h>
#include <Common/Threading/Jobs/JobPriority.h>

namespace ngine::Threading
{
	struct JobRunnerThread;
}

namespace ngine::Rendering
{
	struct LogicalDevice;

	struct SingleUseCommandBuffer
	{
		SingleUseCommandBuffer() = default;
		SingleUseCommandBuffer(
			const Rendering::LogicalDevice& logicalDevice,
			const Rendering::CommandPoolView commandPool,
			Threading::JobRunnerThread& commandBufferCreationThread,
			Rendering::QueueFamily queueFamily,
			Threading::JobPriority priority
		);
		SingleUseCommandBuffer(const SingleUseCommandBuffer&) = delete;
		SingleUseCommandBuffer& operator=(const SingleUseCommandBuffer&) = delete;
		SingleUseCommandBuffer(SingleUseCommandBuffer&& other) = default;
		SingleUseCommandBuffer& operator=(SingleUseCommandBuffer&& other) = default;
		~SingleUseCommandBuffer();

		[[nodiscard]] operator CommandBufferView() const
		{
			return m_unifiedCommandBuffer;
		}
		[[nodiscard]] operator CommandEncoderView() const
		{
			return m_unifiedCommandBuffer;
		}

		Function<void(), 36> OnFinished = []()
		{
		};
	protected:
		Optional<const Rendering::LogicalDevice*> m_pLogicalDevice;
		Rendering::CommandPoolView m_commandPool;
		Threading::JobRunnerThread* m_pCommandBufferCreationThread;
		Rendering::UnifiedCommandBuffer m_unifiedCommandBuffer;
		Rendering::QueueFamily m_queueFamily;
		Threading::JobPriority m_priority;
	};
}
