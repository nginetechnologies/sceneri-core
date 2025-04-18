#pragma once

#include <Renderer/Buffers/StagingBuffer.h>

namespace ngine::Rendering
{
	struct CommandQueueView;
	struct CommandEncoderView;

	struct PerFrameStagingBuffer
	{
		PerFrameStagingBuffer(
			LogicalDevice& logicalDevice,
			const PhysicalDevice& physicalDevice,
			DeviceMemoryPool& memoryPool,
			const size size,
			const EnumFlags<StagingBuffer::Flags> stagingBufferFlags
		);
		PerFrameStagingBuffer(PerFrameStagingBuffer&&) = default;
		PerFrameStagingBuffer& operator=(PerFrameStagingBuffer&&) = default;
		PerFrameStagingBuffer(const PerFrameStagingBuffer&) = delete;
		PerFrameStagingBuffer& operator=(const PerFrameStagingBuffer&) = delete;

		void Destroy(LogicalDeviceView logicalDevice, DeviceMemoryPool& memoryPool);

		void Start();

		void CopyToBuffer(
			LogicalDevice& logicalDevice,
			const Rendering::CommandQueueView commandQueue,
			const Rendering::CommandEncoderView commandEncoder,
			const ConstByteView source,
			const BufferView targetBuffer,
			const size targetBufferOffset = 0
		);

		void CopyToBuffer(
			const Rendering::CommandEncoderView commandEncoder,
			const size sourceBufferOffset,
			const BufferView sourceBuffer,
			const size targetBufferOffset,
			const BufferView targetBuffer,
			const size dataSize
		);

		struct TRIVIAL_ABI BatchCopyContext
		{
			const BufferView m_targetBuffer;
			const size m_targetBufferBaseOffset;
#if !RENDERER_WEBGPU
			const ByteView m_targetData;
#endif
		};
		[[nodiscard]] BatchCopyContext
		BeginBatchCopyToBuffer(const size totalDataSize, const BufferView targetBuffer, const size targetBufferOffset = 0);
		void BatchCopyToBuffer(
			LogicalDevice& logicalDevice,
			BatchCopyContext& context,
			const Rendering::CommandQueueView commandQueue,
			const ConstByteView source,
			const size localOffset
		);
		void EndBatchCopyToBuffer(BatchCopyContext& context, const Rendering::CommandEncoderView commandEncoder);
	private:
		[[nodiscard]] size AcquireBlock(const size requestedSize)
		{
			const size offset = m_stagingBufferOffset;
			Assert(m_stagingBuffer.GetSize() - offset >= requestedSize);
			m_stagingBufferOffset += requestedSize;
			return offset;
		}
#if !RENDERER_WEBGPU
		[[nodiscard]] ByteView AcquireMappedBlock(const size requestedSize)
		{
			const size offset = AcquireBlock(requestedSize);
			const ByteView block = m_stagingBufferMappedData.GetSubView(offset, requestedSize);
			Assert(block.GetDataSize() == requestedSize);
			return block;
		}
#endif
	private:
		StagingBuffer m_stagingBuffer;
#if !RENDERER_WEBGPU
		ByteView m_stagingBufferMappedData;
#endif
		size m_stagingBufferOffset = 0;
	};
}
