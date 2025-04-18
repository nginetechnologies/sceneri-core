#pragma once

#include <Renderer/Buffers/BufferBase.h>
#include <Renderer/Devices/QueueFamily.h>

namespace ngine::Rendering
{
	struct DataToBuffer;

#if RENDERER_METAL
	struct BufferMetal : public BufferBase
	{
		using BufferBase::BufferBase;
	protected:
		void CreateDeviceBuffer(
			const LogicalDeviceView logicalDevice,
			const PhysicalDevice& physicalDevice,
			DeviceMemoryPool& deviceMemoryPool,
			const EnumFlags<UsageFlags> usageFlags,
			const EnumFlags<MemoryFlags> memoryFlags
		);

		void DestroyDeviceBuffer(const LogicalDeviceView logicalDevice, DeviceMemoryPool& deviceMemoryPool);

		void MapAndCopyFrom(
			const LogicalDeviceView logicalDevice,
			const QueueFamily queueFamily,
			const ArrayView<const DataToBuffer> copies,
			const Math::Range<size> bufferRange
		);

		void MapToHostMemoryDeviceBuffer(
			const LogicalDeviceView logicalDevice,
			const Math::Range<size> mappedRange,
			const EnumFlags<MapMemoryFlags> mappedMemoryFlags,
			MapMemoryAsyncCallback&& callback
		);
		[[nodiscard]] bool MapToHostMemoryDeviceBufferAsync(
			const LogicalDeviceView logicalDevice,
			const Math::Range<size> mappedRange,
			const EnumFlags<MapMemoryFlags> mappedMemoryFlags,
			MapMemoryAsyncCallback&& callback
		);

		void UnmapFromHostMemoryDeviceBuffer(const LogicalDeviceView logicalDevice);

		void SetDebugNameDeviceBuffer(const LogicalDevice& logicalDevice, const ConstZeroTerminatedStringView name);
	};

	using DeviceBuffer = BufferMetal;
#endif
}
