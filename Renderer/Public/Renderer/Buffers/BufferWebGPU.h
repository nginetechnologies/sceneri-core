#pragma once

#include <Renderer/Buffers/BufferBase.h>
#include <Renderer/Devices/QueueFamily.h>

namespace ngine::Rendering
{
	struct DataToBuffer;

#if RENDERER_WEBGPU
	struct BufferWebGPU : public BufferBase
	{
		using BufferBase::BufferBase;
	protected:
		void CreateDeviceBuffer(
			LogicalDevice& logicalDevice,
			const PhysicalDevice& physicalDevice,
			DeviceMemoryPool& deviceMemoryPool,
			const EnumFlags<UsageFlags> usageFlags,
			const EnumFlags<MemoryFlags> memoryFlags
		);

		void DestroyDeviceBuffer(const LogicalDeviceView logicalDevice, DeviceMemoryPool& deviceMemoryPool);

		void MapAndCopyFrom(
			const LogicalDevice& logicalDevice,
			const QueueFamily queueFamily,
			const ArrayView<const DataToBuffer> copies,
			const Math::Range<size> bufferRange
		);

		void MapToHostMemoryDeviceBuffer(
			const LogicalDevice& logicalDevice,
			const Math::Range<size> mappedRange,
			const EnumFlags<MapMemoryFlags> mappedMemoryFlags,
			MapMemoryAsyncCallback&& callback
		);
		[[nodiscard]] bool MapToHostMemoryDeviceBufferAsync(
			const LogicalDevice& logicalDevice,
			const Math::Range<size> mappedRange,
			const EnumFlags<MapMemoryFlags> mappedMemoryFlags,
			MapMemoryAsyncCallback&& callback
		);

		void UnmapFromHostMemoryDeviceBuffer(const LogicalDeviceView logicalDevice);
		void SetDebugNameDeviceBuffer(const LogicalDevice& logicalDevice, const ConstZeroTerminatedStringView name);
	};

	using DeviceBuffer = BufferWebGPU;
#endif
}
