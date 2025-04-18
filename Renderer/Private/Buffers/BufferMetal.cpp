#include <Renderer/Buffers/BufferMetal.h>

#include <Renderer/Commands/CommandBufferView.h>
#include <Renderer/Devices/LogicalDevice.h>
#include <Renderer/Devices/PhysicalDevice.h>
#include <Renderer/Commands/CommandQueueView.h>
#include <Renderer/Buffers/DataToBuffer.h>

#include <Renderer/Metal/Includes.h>
#include "Renderer/Metal/GetStorageMode.h"

namespace ngine::Rendering
{
#if RENDERER_METAL
	void BufferMetal::CreateDeviceBuffer(
		[[maybe_unused]] const LogicalDeviceView logicalDevice,
		[[maybe_unused]] const PhysicalDevice& physicalDevice,
		[[maybe_unused]] DeviceMemoryPool& deviceMemoryPool,
		[[maybe_unused]] const EnumFlags<UsageFlags> usageFlags,
		[[maybe_unused]] const EnumFlags<MemoryFlags> memoryFlags
	)
	{
		// TODO: MTLResourceCPUCacheModeWriteCombined instead of MTLResourceCPUCacheModeDefaultCache if we are read only on CPU
		// note: MTLCPUCacheModeDefaultCache & MTLCPUCacheModeWriteCombined in DeviceMemory.cpp
		// TODO: MTLResourceHazardTrackingModeUntracked instead of MTLResourceHazardTrackingModeTracked

		MTLResourceOptions resourceOptions = (Metal::GetStorageMode(memoryFlags) << MTLResourceStorageModeShift) |
		                                     MTLResourceCPUCacheModeDefaultCache | MTLResourceHazardTrackingModeTracked;
		const MTLSizeAndAlign memoryRequirements = [(id<MTLDevice>)logicalDevice heapBufferSizeAndAlignWithLength:m_bufferSize
																																																			options:resourceOptions];

		const PhysicalDevice::MemoryTypeSizeType memoryTypeIndex = physicalDevice.GetMemoryTypeIndex(memoryFlags);
		if (memoryFlags.IsSet(MemoryFlags::HostVisible))
		{
#if PLATFORM_APPLE_VISIONOS
			// No host visible heaps
			m_buffer.m_pBuffer = [(id<MTLDevice>)logicalDevice newBufferWithLength:memoryRequirements.size options:resourceOptions];
			return;
#else
			m_memoryAllocation = deviceMemoryPool.AllocateRaw(logicalDevice, memoryRequirements.size, memoryTypeIndex, memoryFlags);
#endif
		}
		else
		{
#if DISABLE_MEMORY_POOL_USAGE
			m_memoryAllocation = deviceMemoryPool.AllocateRaw(logicalDevice, memoryRequirements.size, memoryTypeIndex, memoryFlags);
#else
			m_memoryAllocation =
				deviceMemoryPool.Allocate(logicalDevice, memoryRequirements.size, (uint32)memoryRequirements.align, memoryTypeIndex, memoryFlags);
#endif
		}

		if (LIKELY(m_memoryAllocation.IsValid()))
		{
			m_buffer.m_pBuffer = [(id<MTLHeap>)m_memoryAllocation.m_memory newBufferWithLength:memoryRequirements.size
																																								 options:resourceOptions
																																									offset:m_memoryAllocation.m_offset];
		}
	}

	void BufferMetal::DestroyDeviceBuffer(
		[[maybe_unused]] const LogicalDeviceView logicalDevice, [[maybe_unused]] DeviceMemoryPool& deviceMemoryPool
	)
	{
		m_buffer.m_pBuffer = nil;
	}

	void BufferMetal::MapAndCopyFrom(
		[[maybe_unused]] const LogicalDeviceView logicalDevice,
		[[maybe_unused]] const QueueFamily queueFamily,
		[[maybe_unused]] const ArrayView<const DataToBuffer> copies,
		[[maybe_unused]] const Math::Range<size> bufferRange
	)
	{
		UNUSED(bufferRange);

		EnumFlags<Flags> flags = m_flags.GetFlags();
		do
		{
			Assert(flags.IsSet(Flags::IsHostMappable), "Memory must be host mappable!");
			Assert(!flags.IsSet(Flags::IsHostMapping), "Memory mapping cannot overlap!");
			Assert(!flags.IsSet(Flags::IsHostMapped), "Memory cannot be simultaneously mapped!");
			if (UNLIKELY_ERROR(flags.IsNotSet(Flags::IsHostMappable) | flags.AreAnySet(Flags::IsHostMapping | Flags::IsHostMapped)))
			{
				return;
			}
		} while (!m_flags.CompareExchangeWeak(flags, flags | Flags::IsHostMapping));

		void* const pMappedMemory = [m_buffer.m_pBuffer contents];

		[[maybe_unused]] const bool wasFlagSet = m_flags.TrySetFlags(Flags::IsHostMapped);
		Assert(wasFlagSet);

		for (const DataToBuffer& __restrict copyToBufferInfo : copies)
		{
			const ByteView target{
				reinterpret_cast<ByteType*>(pMappedMemory) + copyToBufferInfo.targetOffset,
				copyToBufferInfo.source.GetDataSize()
			};
			target.CopyFrom(copyToBufferInfo.source);
		}

		[[maybe_unused]] const EnumFlags<Flags> previousFlags = m_flags.FetchAnd(~(Flags::IsHostMapping | Flags::IsHostMapped));
		Assert(previousFlags.IsSet(Flags::IsHostMapped));
	}

	void BufferMetal::MapToHostMemoryDeviceBuffer(
		[[maybe_unused]] const LogicalDeviceView logicalDevice,
		[[maybe_unused]] const Math::Range<size> mappedRange,
		[[maybe_unused]] const EnumFlags<MapMemoryFlags> mappedMemoryFlags,
		[[maybe_unused]] MapMemoryAsyncCallback&& callback
	)
	{
		[[maybe_unused]] const bool wasFlagSet = m_flags.TrySetFlags(Flags::IsHostMapped);
		Assert(wasFlagSet);

		callback(MapMemoryStatus::Success, ByteView{reinterpret_cast<ByteType*>([m_buffer.m_pBuffer contents]), mappedRange.GetSize()}, false);

#if PLATFORM_APPLE_MACOS || PLATFORM_APPLE_MACCATALYST
		/*if (mappedMemoryFlags.IsSet(MapMemoryFlags::Write))
		{
		  [(id<MTLBuffer>)m_buffer didModifyRange:NSMakeRange(mappedRange.GetMinimum(), mappedRange.GetSize())];
		}*/
#endif

		if (mappedMemoryFlags.IsNotSet(MapMemoryFlags::KeepMapped))
		{
			[[maybe_unused]] const EnumFlags<Flags> previousFlags = m_flags.FetchAnd(~(Flags::IsHostMapping | Flags::IsHostMapped));
			Assert(previousFlags.IsSet(Flags::IsHostMapped));
		}
	}

	bool BufferMetal::MapToHostMemoryDeviceBufferAsync(
		const LogicalDeviceView logicalDevice,
		const Math::Range<size> mappedRange,
		const EnumFlags<MapMemoryFlags> mappedMemoryFlags,
		MapMemoryAsyncCallback&& callback
	)
	{
		MapToHostMemoryDeviceBuffer(logicalDevice, mappedRange, mappedMemoryFlags, Forward<MapMemoryAsyncCallback>(callback));
		// Indicate synchronous execution
		return false;
	}

	void BufferMetal::UnmapFromHostMemoryDeviceBuffer([[maybe_unused]] const LogicalDeviceView logicalDevice)
	{
	}

	void BufferMetal::SetDebugNameDeviceBuffer(
		[[maybe_unused]] const LogicalDevice& logicalDevice, [[maybe_unused]] const ConstZeroTerminatedStringView name
	)
	{
		[m_buffer.m_pBuffer setLabel:[NSString stringWithUTF8String:name]];
	}
#endif
}
