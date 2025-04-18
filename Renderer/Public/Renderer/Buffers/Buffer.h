#pragma once

#include <Renderer/Constants.h>
#include <Renderer/Buffers/BufferView.h>
#include <Renderer/Devices/MemoryFlags.h>
#include <Renderer/Buffers/DeviceMemory.h>
#include <Renderer/Buffers/DataToBuffer.h>
#include <Renderer/Devices/DeviceMemoryAllocation.h>
#include <Renderer/Devices/QueueFamily.h>

#include <Common/AtomicEnumFlags.h>
#include <Common/Memory/Containers/ForwardDeclarations/ZeroTerminatedStringView.h>
#include <Common/Memory/Containers/ByteView.h>
#include <Common/Function/ForwardDeclarations/Function.h>
#include <Common/Math/ForwardDeclarations/Range.h>

#include <Renderer/Buffers/BufferBase.h>
#include <Renderer/Buffers/BufferVulkan.h>
#include <Renderer/Buffers/BufferMetal.h>
#include <Renderer/Buffers/BufferWebGPU.h>

namespace ngine::Rendering
{
	struct LogicalDevice;

	struct Buffer : public DeviceBuffer
	{
		Buffer() = default;
		Buffer(const Buffer&) = delete;
		Buffer& operator=(const Buffer&) = delete;

		Buffer(
			LogicalDevice& logicalDevice,
			const PhysicalDevice& physicalDevice,
			DeviceMemoryPool& deviceMemoryPool,
			const size size,
			const EnumFlags<UsageFlags> usageFlags,
			const EnumFlags<MemoryFlags> memoryFlags
		);

		Buffer(Buffer&& other) noexcept
		{
			m_buffer = other.m_buffer;

#if RENDERER_HAS_DEVICE_MEMORY
			m_memoryAllocation = other.m_memoryAllocation;
#endif

			m_bufferSize = other.m_bufferSize;
			m_flags = other.m_flags.FetchClear();

#if RENDERER_VULKAN || RENDERER_METAL || RENDERER_WEBGPU
			other.m_buffer.m_pBuffer = 0;
#endif

#if RENDERER_HAS_DEVICE_MEMORY
			other.m_memoryAllocation.m_memory = {};
#endif

			other.m_bufferSize = 0;
		}

		Buffer& operator=(Buffer&& other) noexcept;
		~Buffer();

		void Destroy(const LogicalDeviceView logicalDevice, DeviceMemoryPool& deviceMemoryPool);

#if RENDERER_HAS_DEVICE_MEMORY
		[[nodiscard]] DeviceMemoryView GetBoundMemory() const
		{
			return m_memoryAllocation.m_memory;
		}
#endif

		[[nodiscard]] size GetOffset() const
		{
#if RENDERER_HAS_DEVICE_MEMORY
			return m_memoryAllocation.m_offset;
#else
			return 0;
#endif
		}
		[[nodiscard]] size GetSize() const
		{
			return m_bufferSize;
		}

		[[nodiscard]] bool IsValid() const
		{
#if RENDERER_VULKAN
			return m_memoryAllocation.IsValid() & m_buffer.IsValid();
#else
			return m_buffer.IsValid();
#endif
		}

		[[nodiscard]] operator BufferView() const
		{
#if RENDERER_VULKAN
			if (m_memoryAllocation.IsValid())
			{
				return m_buffer;
			}
			return {};
#else
			return m_buffer;
#endif
		}

#if RENDERER_VULKAN
		[[nodiscard]] operator VkBuffer() const LIFETIME_BOUND
		{
			return m_buffer;
		}
#elif RENDERER_METAL
		[[nodiscard]] operator id<MTLBuffer>() const LIFETIME_BOUND
		{
			return m_buffer;
		}
#elif RENDERER_WEBGPU
		[[nodiscard]] operator WGPUBuffer() const LIFETIME_BOUND
		{
			return m_buffer;
		}
#endif

		void SetDebugName(const LogicalDevice& logicalDevice, const ConstZeroTerminatedStringView name);

		void MapToHostMemory(
			const LogicalDevice& logicalDevice,
			const Math::Range<size> mappedRange,
			const EnumFlags<MapMemoryFlags> flags,
			MapMemoryAsyncCallback&& callback
		);
		//! Returns true if the operation was queued asynchonously
		//! false indicates immediate mapping
		[[nodiscard]] bool MapToHostMemoryAsync(
			const LogicalDevice& logicalDevice,
			const Math::Range<size> mappedRange,
			const EnumFlags<MapMemoryFlags> flags,
			MapMemoryAsyncCallback&& callback
		);
		void UnmapFromHostMemory(const LogicalDeviceView logicalDevice);

		//! Maps this buffer to host temporarily and copies the specified regions of CPU memory into it
		void MapAndCopyFrom(
			const LogicalDevice& logicalDevice,
			const QueueFamily queueFamily,
			const ArrayView<const DataToBuffer> copies,
			const Math::Range<size> bufferRange
		);

		[[nodiscard]] uint64 GetDeviceAddress(const LogicalDevice& logicalDevice) const;
	};
}
