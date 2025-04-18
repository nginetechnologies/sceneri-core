#include "Stages/PerFrameStagingBuffer.h"

#include <Renderer/Commands/CommandQueueView.h>
#include <Renderer/Commands/CommandEncoderView.h>
#include <Renderer/Commands/BlitCommandEncoder.h>
#include <Renderer/Devices/LogicalDevice.h>
#include <Renderer/Jobs/QueueSubmissionJob.h>

#include <Renderer/WebGPU/Includes.h>

#include <Common/Threading/Jobs/JobRunnerThread.h>

namespace ngine::Rendering
{
	PerFrameStagingBuffer::PerFrameStagingBuffer(
		[[maybe_unused]] LogicalDevice& logicalDevice,
		[[maybe_unused]] const PhysicalDevice& physicalDevice,
		[[maybe_unused]] DeviceMemoryPool& memoryPool,
		[[maybe_unused]] const size allocatedSize,
		[[maybe_unused]] const EnumFlags<StagingBuffer::Flags> stagingBufferFlags
	)
		: m_stagingBuffer(logicalDevice, physicalDevice, memoryPool, allocatedSize, stagingBufferFlags)
	{
#if !RENDERER_WEBGPU
		m_stagingBuffer.MapToHostMemory(
			logicalDevice,
			Math::Range<size>::Make(0, allocatedSize),
			Buffer::MapMemoryFlags::Write | Buffer::MapMemoryFlags::KeepMapped,
			[this]([[maybe_unused]] const Buffer::MapMemoryStatus status, const ByteView data, [[maybe_unused]] const bool executedAsynchronously)
			{
				Assert(status == Buffer::MapMemoryStatus::Success);
				Assert(!executedAsynchronously);
				m_stagingBufferMappedData = data;
			}
		);
#endif
	}

	void PerFrameStagingBuffer::Destroy([[maybe_unused]] LogicalDeviceView logicalDevice, [[maybe_unused]] DeviceMemoryPool& memoryPool)
	{
#if !RENDERER_WEBGPU
		m_stagingBuffer.UnmapFromHostMemory(logicalDevice);
#endif
		m_stagingBuffer.Destroy(logicalDevice, memoryPool);
	}

	void PerFrameStagingBuffer::Start()
	{
		m_stagingBufferOffset = 0;
	}

	void PerFrameStagingBuffer::CopyToBuffer(
		[[maybe_unused]] LogicalDevice& logicalDevice,
		[[maybe_unused]] const Rendering::CommandQueueView commandQueue,
		[[maybe_unused]] const Rendering::CommandEncoderView commandEncoder,
		const ConstByteView source,
		const BufferView targetBuffer,
		const size targetBufferOffset
	)
	{
#if RENDERER_WEBGPU
		Threading::Atomic<bool> finished{false};
		logicalDevice.GetQueueSubmissionJob(QueueFamily::Graphics)
			.QueueCallback(
				[commandQueue, targetBuffer, targetBufferOffset, source, &finished]()
				{
					wgpuQueueWriteBuffer(commandQueue, targetBuffer, targetBufferOffset, source.GetData(), source.GetDataSize());
					finished = true;
				}
			);
		Threading::JobRunnerThread& thread = *Threading::JobRunnerThread::GetCurrent();
		while (!finished)
		{
			thread.DoRunNextJob();
		}
#else
		ByteView target = AcquireMappedBlock(source.GetDataSize());
		target.CopyFrom(source);
		const size stagingBufferOffset = m_stagingBufferMappedData.GetIteratorIndex(target.GetData());

		const BlitCommandEncoder blitCommandEncoder = commandEncoder.BeginBlit();
		blitCommandEncoder.RecordCopyBufferToBuffer(
			m_stagingBuffer,
			targetBuffer,
			Array{BufferCopy{stagingBufferOffset, targetBufferOffset, source.GetDataSize()}}
		);
#endif
	}

	void PerFrameStagingBuffer::CopyToBuffer(
		const Rendering::CommandEncoderView commandEncoder,
		const size sourceBufferOffset,
		const BufferView sourceBuffer,
		const size targetBufferOffset,
		const BufferView targetBuffer,
		const size dataSize
	)
	{
		BlitCommandEncoder blitCommandEncoder = commandEncoder.BeginBlit();
#if RENDERER_WEBGPU
		if (sourceBuffer == targetBuffer)
		{
			// WebGPU doesn't support same buffer copies.
			// First copy into the staging buffer, and then to the final target
			const size stagingBufferOffset = AcquireBlock(dataSize);
			blitCommandEncoder.RecordCopyBufferToBuffer(
				sourceBuffer,
				m_stagingBuffer,
				ArrayView<const BufferCopy, uint16>(BufferCopy{sourceBufferOffset, stagingBufferOffset, dataSize})
			);

			blitCommandEncoder.RecordCopyBufferToBuffer(
				m_stagingBuffer,
				targetBuffer,
				ArrayView<const BufferCopy, uint16>(BufferCopy{stagingBufferOffset, targetBufferOffset, dataSize})
			);
			return;
		}
#endif

		blitCommandEncoder.RecordCopyBufferToBuffer(
			sourceBuffer,
			targetBuffer,
			ArrayView<const BufferCopy, uint16>(BufferCopy{sourceBufferOffset, targetBufferOffset, dataSize})
		);
	}

	PerFrameStagingBuffer::BatchCopyContext PerFrameStagingBuffer::BeginBatchCopyToBuffer(
		[[maybe_unused]] const size totalDataSize, const BufferView targetBuffer, const size targetBufferOffset
	)
	{
#if RENDERER_WEBGPU
		return BatchCopyContext{targetBuffer, targetBufferOffset};
#else
		return BatchCopyContext{targetBuffer, targetBufferOffset, AcquireMappedBlock(totalDataSize)};
#endif
	}

	void PerFrameStagingBuffer::BatchCopyToBuffer(
		[[maybe_unused]] LogicalDevice& logicalDevice,
		[[maybe_unused]] BatchCopyContext& context,
		[[maybe_unused]] const Rendering::CommandQueueView commandQueue,
		const ConstByteView source,
		const size localOffset
	)
	{
#if RENDERER_WEBGPU
		logicalDevice.GetQueueSubmissionJob(QueueFamily::Graphics)
			.QueueCallback(
				[commandQueue,
		     context,
		     localOffset,
		     source = InlineVector<ByteType, 64>{ArrayView<const ByteType, size>{source.GetData(), source.GetDataSize()}}]()
				{
					wgpuQueueWriteBuffer(
						commandQueue,
						context.m_targetBuffer,
						context.m_targetBufferBaseOffset + localOffset,
						source.GetData(),
						source.GetDataSize()
					);
				}
			);
#else
		[[maybe_unused]] const bool wasCopied = context.m_targetData.GetSubView(localOffset, source.GetDataSize()).CopyFrom(source);
		Assert(wasCopied);
#endif
	}

	void PerFrameStagingBuffer::EndBatchCopyToBuffer(
		[[maybe_unused]] BatchCopyContext& context, [[maybe_unused]] const Rendering::CommandEncoderView commandEncoder
	)
	{
#if !RENDERER_WEBGPU
		const size stagingBufferOffset = m_stagingBufferMappedData.GetIteratorIndex(context.m_targetData.GetData());

		const BlitCommandEncoder blitCommandEncoder = commandEncoder.BeginBlit();
		blitCommandEncoder.RecordCopyBufferToBuffer(
			m_stagingBuffer,
			context.m_targetBuffer,
			Array{BufferCopy{stagingBufferOffset, context.m_targetBufferBaseOffset, context.m_targetData.GetDataSize()}}
		);
#endif
	}
}
