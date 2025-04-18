#include <Renderer/Buffers/Buffer.h>

#include <Renderer/Buffers/StagingBuffer.h>
#include <Renderer/Buffers/VertexBuffer.h>
#include <Renderer/Buffers/IndexBuffer.h>
#include <Renderer/Buffers/UniformBuffer.h>
#include <Renderer/Buffers/StorageBuffer.h>
#include <Renderer/Commands/CommandBufferView.h>
#include <Renderer/Devices/LogicalDevice.h>
#include <Renderer/Devices/PhysicalDevice.h>
#include <Renderer/Commands/CommandQueueView.h>

#include <Renderer/Vulkan/Includes.h>
#include <Renderer/Metal/Includes.h>
#include <Renderer/WebGPU/Includes.h>
#include <Renderer/Metal/GetStorageMode.h>

#include "Devices/LogicalDevice.h"

#include "Buffers/DeviceMemory.h"

#include <Common/Memory/Containers/ZeroTerminatedStringView.h>

namespace ngine::Rendering
{
	uint64 BufferView::GetDeviceAddress([[maybe_unused]] const LogicalDevice& logicalDevice) const
	{
#if RENDERER_VULKAN
		const VkBufferDeviceAddressInfoKHR bufferDeviceAddressInfo{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, m_pBuffer};
		const PFN_vkGetBufferDeviceAddress getBufferDeviceAddress =
			reinterpret_cast<PFN_vkGetBufferDeviceAddress>(logicalDevice.GetVkGetBufferDeviceAddress());
		return getBufferDeviceAddress(logicalDevice, &bufferDeviceAddressInfo);
#elif RENDERER_METAL
		if (@available(iOS 16, macOS 13, *))
		{
			return [m_pBuffer gpuAddress];
		}
		else
		{
			Assert(false, "Not supported");
			return 0;
		}
#elif RENDERER_WEBGPU
		Assert(false, "Not supported");
		return 0;
#endif
	}

	Buffer::Buffer(
		LogicalDevice& logicalDevice,
		[[maybe_unused]] const PhysicalDevice& physicalDevice,
		[[maybe_unused]] DeviceMemoryPool& deviceMemoryPool,
		const size requestedSize,
		[[maybe_unused]] const EnumFlags<UsageFlags> usageFlags,
		const EnumFlags<MemoryFlags> memoryFlags
	)
		: DeviceBuffer(requestedSize, Flags::IsHostMappable * memoryFlags.IsSet(MemoryFlags::HostVisible))
	{
		Assert(m_bufferSize > 0);

		CreateDeviceBuffer(logicalDevice, physicalDevice, deviceMemoryPool, usageFlags, memoryFlags);
	}

	Buffer& Buffer::operator=(Buffer&& other) noexcept
	{
		Assert(!IsValid(), "Destroy must be called before the buffer is destroyed!");
		m_buffer = other.m_buffer;

		other.m_buffer.m_pBuffer = 0;

#if RENDERER_HAS_DEVICE_MEMORY
		m_memoryAllocation = other.m_memoryAllocation;
		other.m_memoryAllocation.m_memory = {};
#endif
		m_flags = other.m_flags.FetchClear();
		m_bufferSize = other.m_bufferSize;
		other.m_bufferSize = 0;
		return *this;
	}

	Buffer::~Buffer()
	{
		Assert(m_flags.IsNotSet(Flags::IsHostMapping), "Buffer can't be destroyed while mapping to host!");
		Assert(m_flags.IsNotSet(Flags::IsHostMapped), "Buffer must be unmapped before being destroyed!");

#if RENDERER_VULKAN
		Assert(!m_buffer.IsValid() && !m_memoryAllocation.IsValid(), "Destroy must be called before the buffer is destroyed!");
#else
		Assert(!IsValid(), "Destroy must be called before the buffer is destroyed!");
#endif
	}

	void Buffer::Destroy([[maybe_unused]] const LogicalDeviceView logicalDevice, [[maybe_unused]] DeviceMemoryPool& deviceMemoryPool)
	{
		Assert(m_flags.AreNoneSet(Flags::IsHostMapping | Flags::IsHostMapped), "Buffer must be unmapped before being destroyed!");

		DestroyDeviceBuffer(logicalDevice, deviceMemoryPool);

		m_flags &= ~Flags::IsHostMappable;
	}

	void Buffer::SetDebugName([[maybe_unused]] const LogicalDevice& logicalDevice, const ConstZeroTerminatedStringView name)
	{
		SetDebugNameDeviceBuffer(logicalDevice, name);
	}

	void Buffer::MapToHostMemory(
		[[maybe_unused]] const LogicalDevice& logicalDevice,
		const Math::Range<size> mappedRange,
		const EnumFlags<MapMemoryFlags> mappedMemoryFlags,
		MapMemoryAsyncCallback&& callback
	)
	{
		Assert(mappedRange.GetMaximum() <= m_bufferSize);

		EnumFlags<Flags> flags = m_flags.GetFlags();
		do
		{
			Assert(flags.IsSet(Flags::IsHostMappable), "Memory must be host mappable!");
			Assert(!flags.IsSet(Flags::IsHostMapping), "Memory mapping cannot overlap!");
			Assert(!flags.IsSet(Flags::IsHostMapped), "Memory cannot be simultaneously mapped!");
			if (UNLIKELY_ERROR(flags.IsNotSet(Flags::IsHostMappable) | flags.AreAnySet(Flags::IsHostMapping | Flags::IsHostMapped)))
			{
				callback(MapMemoryStatus::MapFailed, {}, false);
				return;
			}
		} while (!m_flags.CompareExchangeWeak(flags, flags | Flags::IsHostMapping));

		MapToHostMemoryDeviceBuffer(logicalDevice, mappedRange, mappedMemoryFlags, Move(callback));
	}

	bool Buffer::MapToHostMemoryAsync(
		[[maybe_unused]] const LogicalDevice& logicalDevice,
		const Math::Range<size> mappedRange,
		const EnumFlags<MapMemoryFlags> mappedMemoryFlags,
		MapMemoryAsyncCallback&& callback
	)
	{
		Assert(mappedRange.GetMaximum() <= m_bufferSize);

		EnumFlags<Flags> flags = m_flags.GetFlags();
		do
		{
			Assert(flags.IsSet(Flags::IsHostMappable), "Memory must be host mappable!");
			Assert(!flags.IsSet(Flags::IsHostMapping), "Memory mapping cannot overlap!");
			Assert(!flags.IsSet(Flags::IsHostMapped), "Memory cannot be simultaneously mapped!");
			if (UNLIKELY_ERROR(flags.IsNotSet(Flags::IsHostMappable) | flags.AreAnySet(Flags::IsHostMapping | Flags::IsHostMapped)))
			{
				callback(MapMemoryStatus::MapFailed, {}, false);
				return false;
			}
		} while (!m_flags.CompareExchangeWeak(flags, flags | Flags::IsHostMapping));

		return MapToHostMemoryDeviceBufferAsync(logicalDevice, mappedRange, mappedMemoryFlags, Move(callback));
	}

	void Buffer::UnmapFromHostMemory([[maybe_unused]] const LogicalDeviceView logicalDevice)
	{
		const EnumFlags<Flags> previousFlags = m_flags.FetchAnd(~(Flags::IsHostMapping | Flags::IsHostMapped));
		if (previousFlags.AreAnySet(Flags::IsHostMapping | Flags::IsHostMapped))
		{
			UnmapFromHostMemoryDeviceBuffer(logicalDevice);
		}
	}

	void Buffer::MapAndCopyFrom(
		const LogicalDevice& logicalDevice,
		const QueueFamily queueFamily,
		const ArrayView<const DataToBuffer> copies,
		const Math::Range<size> bufferRange
	)
	{
		Assert(m_flags.IsSet(Flags::IsHostMappable));

		Assert(copies.All(
			[bufferRange](const DataToBuffer& __restrict copyToBufferInfo)
			{
				const Math::Range<size> copyRange =
					Math::Range<size>::Make(bufferRange.GetMinimum() + copyToBufferInfo.targetOffset, copyToBufferInfo.source.GetDataSize());
				return bufferRange.Contains(copyRange);
			}
		));

		DeviceBuffer::MapAndCopyFrom(logicalDevice, queueFamily, copies, bufferRange);
	}

	uint64 Buffer::GetDeviceAddress(const LogicalDevice& logicalDevice) const
	{
		return m_buffer.GetDeviceAddress(logicalDevice);
	}

	StagingBuffer::StagingBuffer(
		LogicalDevice& logicalDevice,
		const PhysicalDevice& physicalDevice,
		DeviceMemoryPool& memoryPool,
		const size size,
		const EnumFlags<Flags> flags
	)
		: Buffer(
				logicalDevice,
				physicalDevice,
				memoryPool,
				size,
				Buffer::UsageFlags::TransferSource * flags.IsSet(Flags::TransferSource) |
					Buffer::UsageFlags::TransferDestination * flags.IsSet(Flags::TransferDestination),
				MemoryFlags::HostCoherent | MemoryFlags::HostVisible
			)
	{
		Assert(flags.AreAnySet(Flags::TransferSource | Flags::TransferDestination));
	}

	VertexBuffer::VertexBuffer(
		LogicalDevice& logicalDevice,
		const PhysicalDevice& physicalDevice,
		DeviceMemoryPool& memoryPool,
		const size size,
		const EnumFlags<UsageFlags> usageFlags,
		const bool allowCpuAccess
	)
		: Buffer(
				logicalDevice,
				physicalDevice,
				memoryPool,
				size,
				usageFlags | Buffer::UsageFlags::VertexBuffer | Buffer::UsageFlags::TransferDestination |
					Buffer::UsageFlags::AccelerationStructureBuildInputReadOnly | Buffer::UsageFlags::StorageBuffer |
					Buffer::UsageFlags::ShaderDeviceAddress,
				((allowCpuAccess && !RENDERER_WEBGPU) ? MemoryFlags::HostCoherent | MemoryFlags::HostVisible : MemoryFlags::DeviceLocal) |
					MemoryFlags::AllocateDeviceAddress
			)
	{
	}

	IndexBuffer::IndexBuffer(
		LogicalDevice& logicalDevice, const PhysicalDevice& physicalDevice, DeviceMemoryPool& memoryPool, const size size
	)
		: Buffer(
				logicalDevice,
				physicalDevice,
				memoryPool,
				size,
				Buffer::UsageFlags::IndexBuffer | Buffer::UsageFlags::TransferDestination |
					Buffer::UsageFlags::AccelerationStructureBuildInputReadOnly | Buffer::UsageFlags::StorageBuffer |
					Buffer::UsageFlags::ShaderDeviceAddress,
				MemoryFlags::DeviceLocal | MemoryFlags::AllocateDeviceAddress
			)
	{
	}

	UniformBuffer::UniformBuffer(
		LogicalDevice& logicalDevice, const PhysicalDevice& physicalDevice, DeviceMemoryPool& memoryPool, const size size
	)
		: Buffer(
				logicalDevice,
				physicalDevice,
				memoryPool,
				size,
				Buffer::UsageFlags::UniformBuffer | Buffer::UsageFlags::TransferDestination,
				MemoryFlags::DeviceLocal
			)
	{
	}

	StorageBuffer::StorageBuffer(
		LogicalDevice& logicalDevice,
		const PhysicalDevice& physicalDevice,
		DeviceMemoryPool& memoryPool,
		const size size,
		const EnumFlags<Flags> flags
	)
		: Buffer(
				logicalDevice,
				physicalDevice,
				memoryPool,
				size,
				Buffer::UsageFlags::StorageBuffer | Buffer::UsageFlags::TransferSource * flags.IsSet(Flags::TransferSource) |
					Buffer::UsageFlags::TransferDestination * flags.IsSet(Flags::TransferDestination),
				MemoryFlags::DeviceLocal
			)
	{
	}
}
