#pragma once

#include <Engine/Entity/RenderItemIdentifier.h>

#include <Renderer/Buffers/Buffer.h>

#include <Common/Storage/Identifier.h>

namespace ngine::Rendering
{
	struct LogicalDevice;
	struct CommandEncoderView;
	struct PerFrameStagingBuffer;

	struct InstanceBuffer
	{
		using InstanceIndexType = Entity::RenderItemIdentifier::IndexType;

		InstanceBuffer(
			LogicalDevice& logicalDevice,
			const uint32 maximumInstanceCount,
			const uint32 elementDataSize,
			const EnumFlags<Buffer::UsageFlags> bufferUsageFlags
		);
		InstanceBuffer(const InstanceBuffer&) = delete;
		InstanceBuffer& operator=(const InstanceBuffer&) = delete;
		InstanceBuffer(InstanceBuffer&&) = default;
		InstanceBuffer& operator=(InstanceBuffer&&) = default;

		void Destroy(LogicalDevice& logicalDevice);
		void MoveElement(
			const uint32 sourceInstanceIndex,
			const InstanceIndexType targetInstanceIndex,
			const CommandEncoderView graphicsCommandEncoder,
			const uint32 elementDataSize,
			PerFrameStagingBuffer& perFrameStagingBuffer
		);

		void StartBatchUpload(
			const InstanceIndexType firstInstanceIndex,
			const InstanceIndexType instanceCount,
			const CommandEncoderView graphicsCommandEncoder,
			const uint32 elementDataSize
		);
		void EndBatchUpload(
			const InstanceIndexType firstInstanceIndex,
			const InstanceIndexType instanceCount,
			const CommandEncoderView graphicsCommandEncoder,
			const uint32 elementDataSize
		);

		[[nodiscard]] bool IsValid() const
		{
			return m_buffer.IsValid();
		}
		[[nodiscard]] BufferView GetBuffer() const
		{
			return m_buffer;
		}

		[[nodiscard]] inline InstanceIndexType AddInstance()
		{
			if (m_firstInstanceIndex > 0)
			{
				m_firstInstanceIndex--;
				m_instanceCount++;
				return m_firstInstanceIndex;
			}

			if (m_instanceCount == m_capacity)
			{
				return m_capacity;
			}
			const InstanceIndexType instanceIndex = m_instanceCount++;
			Assert((instanceIndex >= m_firstInstanceIndex) & (instanceIndex - m_firstInstanceIndex < m_instanceCount));

			return instanceIndex;
		}

		enum class RemovalResult : uint8
		{
			RemovedFrontInstance,
			RemovedMiddleInstance,
			RemovedBackInstance,
			RemovedLastRemainingInstance,
		};
		[[nodiscard]] RemovalResult RemoveInstance(const InstanceIndexType instanceIndex)
		{
			const InstanceIndexType previousInstanceCount = m_instanceCount--;
			const InstanceIndexType firstInstanceIndex = m_firstInstanceIndex;

			if (previousInstanceCount == 1)
			{
				return RemovalResult::RemovedLastRemainingInstance;
			}
			else if (instanceIndex == firstInstanceIndex)
			{
				m_firstInstanceIndex = firstInstanceIndex + 1;
				return RemovalResult::RemovedFrontInstance;
			}
			else if (instanceIndex == firstInstanceIndex + previousInstanceCount - 1u)
			{
				return RemovalResult::RemovedBackInstance;
			}
			else
			{
				return RemovalResult::RemovedMiddleInstance;
			}
		}

		[[nodiscard]] inline InstanceIndexType GetCapacity() const
		{
			return m_capacity;
		}
		[[nodiscard]] inline InstanceIndexType GetFirstInstanceIndex() const
		{
			return m_firstInstanceIndex;
		}
		[[nodiscard]] inline InstanceIndexType GetInstanceCount() const
		{
			return m_instanceCount;
		}
	protected:
		Buffer m_buffer;
		InstanceIndexType m_capacity;
		InstanceIndexType m_firstInstanceIndex = 0;
		InstanceIndexType m_instanceCount = 0;
	};
}
