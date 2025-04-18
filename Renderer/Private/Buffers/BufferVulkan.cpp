#include <Renderer/Buffers/BufferVulkan.h>

#include <Renderer/Commands/CommandBufferView.h>
#include <Renderer/Devices/LogicalDevice.h>
#include <Renderer/Devices/PhysicalDevice.h>
#include <Renderer/Commands/CommandQueueView.h>
#include <Renderer/Buffers/DataToBuffer.h>

#include <Renderer/Vulkan/Includes.h>

namespace ngine::Rendering
{
#if RENDERER_VULKAN
	BufferVulkan::~BufferVulkan()
	{
		Assert(!m_buffer.IsValid() && !m_memoryAllocation.IsValid(), "Destroy must be called before the buffer is destroyed!");
	}

	void BufferVulkan::CreateDeviceBuffer(
		[[maybe_unused]] const LogicalDeviceView logicalDevice,
		[[maybe_unused]] const PhysicalDevice& physicalDevice,
		[[maybe_unused]] DeviceMemoryPool& deviceMemoryPool,
		[[maybe_unused]] const EnumFlags<UsageFlags> usageFlags,
		[[maybe_unused]] const EnumFlags<MemoryFlags> memoryFlags
	)
	{
		{
			const VkBufferCreateInfo bufferInfo = {
				VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
				nullptr,
				0,
				m_bufferSize,
				static_cast<VkBufferUsageFlags>(usageFlags.GetFlags()),
				VK_SHARING_MODE_EXCLUSIVE,
				0,
				nullptr
			};

			const VkResult result = vkCreateBuffer(logicalDevice, &bufferInfo, nullptr, &m_buffer.m_pBuffer);
			Assert(result == VK_SUCCESS);
			if (UNLIKELY(result != VK_SUCCESS))
			{
				Assert(m_buffer.m_pBuffer == 0);
				return;
			}
		}
		size requiredMemorySize;
		uint32 requiredMemoryAlignment;
		uint32 requiredMemoryTypes;
		{
			VkMemoryRequirements memoryRequirements;
			vkGetBufferMemoryRequirements(logicalDevice, m_buffer.m_pBuffer, &memoryRequirements);
			requiredMemorySize = memoryRequirements.size;
			requiredMemoryAlignment = (uint32)memoryRequirements.alignment;
			requiredMemoryTypes = memoryRequirements.memoryTypeBits;
		}

		const PhysicalDevice::MemoryTypeSizeType memoryTypeIndex = physicalDevice.GetMemoryTypeIndex(memoryFlags, requiredMemoryTypes);
		if (memoryFlags.IsSet(MemoryFlags::HostVisible))
		{
			m_memoryAllocation = deviceMemoryPool.AllocateRaw(logicalDevice, requiredMemorySize, memoryTypeIndex, memoryFlags);
		}
		else
		{
#if DISABLE_MEMORY_POOL_USAGE
			m_memoryAllocation = deviceMemoryPool.AllocateRaw(logicalDevice, requiredMemorySize, memoryTypeIndex, memoryFlags);
#else
			m_memoryAllocation =
				deviceMemoryPool.Allocate(logicalDevice, requiredMemorySize, requiredMemoryAlignment, memoryTypeIndex, memoryFlags);
#endif
		}
		if (LIKELY(m_memoryAllocation.m_memory.IsValid()))
		{
			vkBindBufferMemory(logicalDevice, m_buffer.m_pBuffer, m_memoryAllocation.m_memory, m_memoryAllocation.m_offset);
		}
	}

	void BufferVulkan::DestroyDeviceBuffer(
		[[maybe_unused]] const LogicalDeviceView logicalDevice, [[maybe_unused]] DeviceMemoryPool& deviceMemoryPool
	)
	{
		vkDestroyBuffer(logicalDevice, m_buffer.m_pBuffer, nullptr);
		m_buffer.m_pBuffer = 0;

		if (m_memoryAllocation.m_memory.IsValid())
		{
			if (m_flags.IsSet(Flags::IsHostMappable))
			{
				Assert(deviceMemoryPool.IsRawAllocationValid(m_memoryAllocation));
				deviceMemoryPool.DeallocateRaw(logicalDevice, m_memoryAllocation);
			}
			else
			{
				Assert(deviceMemoryPool.IsAllocationValid(m_memoryAllocation));
				deviceMemoryPool.Deallocate(m_memoryAllocation);
			}
			m_memoryAllocation.m_memory = {};
		}
	}

	void BufferVulkan::MapAndCopyFrom(
		[[maybe_unused]] const LogicalDeviceView logicalDevice,
		[[maybe_unused]] const QueueFamily queueFamily,
		[[maybe_unused]] const ArrayView<const DataToBuffer> copies,
		[[maybe_unused]] const Math::Range<size> bufferRange
	)
	{
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

		void* pMappedMemory;
		const VkResult result = vkMapMemory(
			logicalDevice,
			m_memoryAllocation.m_memory,
			m_memoryAllocation.m_offset + bufferRange.GetMinimum(),
			bufferRange.GetSize(),
			0,
			&pMappedMemory
		);
		Assert(result == VK_SUCCESS);
		if (LIKELY(result == VK_SUCCESS))
		{
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

			const EnumFlags<Flags> previousFlags = m_flags.FetchAnd(~(Flags::IsHostMapping | Flags::IsHostMapped));
			Assert(previousFlags.IsSet(Flags::IsHostMapped));
			if (LIKELY(previousFlags.IsSet(Flags::IsHostMapped)))
			{
				vkUnmapMemory(logicalDevice, m_memoryAllocation.m_memory);
			}
		}
		else
		{
			[[maybe_unused]] const bool wasFlagCleared = m_flags.TryClearFlags(Flags::IsHostMapping);
			Assert(wasFlagCleared);
		}
	}

	void BufferVulkan::MapToHostMemoryDeviceBuffer(
		[[maybe_unused]] const LogicalDeviceView logicalDevice,
		[[maybe_unused]] const Math::Range<size> mappedRange,
		[[maybe_unused]] const EnumFlags<MapMemoryFlags> mappedMemoryFlags,
		[[maybe_unused]] MapMemoryAsyncCallback&& callback
	)
	{
		UNUSED(mappedMemoryFlags);

		Assert(mappedRange.GetMinimum() >= m_memoryAllocation.m_offset);
		Assert(mappedRange.GetMaximum() <= m_memoryAllocation.m_offset + m_memoryAllocation.m_size || m_memoryAllocation.m_size == 0);
		Assert(mappedRange.GetSize() <= m_memoryAllocation.m_size);

		void* pMappedMemory;
		const VkResult result = vkMapMemory(
			logicalDevice,
			m_memoryAllocation.m_memory,
			m_memoryAllocation.m_offset + mappedRange.GetMinimum(),
			mappedRange.GetSize(),
			0,
			&pMappedMemory
		);
		Assert(result == VK_SUCCESS);
		if (LIKELY(result == VK_SUCCESS))
		{
			[[maybe_unused]] const bool wasFlagSet = m_flags.TrySetFlags(Flags::IsHostMapped);
			Assert(wasFlagSet);

			callback(MapMemoryStatus::Success, ByteView{reinterpret_cast<ByteType*>(pMappedMemory), mappedRange.GetSize()}, false);

			if (mappedMemoryFlags.IsNotSet(MapMemoryFlags::KeepMapped))
			{
				const EnumFlags<Flags> previousFlags = m_flags.FetchAnd(~(Flags::IsHostMapping | Flags::IsHostMapped));
				Assert(previousFlags.IsSet(Flags::IsHostMapped));
				if (LIKELY(previousFlags.IsSet(Flags::IsHostMapped)))
				{
					vkUnmapMemory(logicalDevice, m_memoryAllocation.m_memory);
				}
			}
		}
		else
		{
			[[maybe_unused]] const bool wasFlagCleared = m_flags.TryClearFlags(Flags::IsHostMapping);
			Assert(wasFlagCleared);

			callback(MapMemoryStatus::MapFailed, {}, false);
		}
	}

	bool BufferVulkan::MapToHostMemoryDeviceBufferAsync(
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

	void BufferVulkan::UnmapFromHostMemoryDeviceBuffer([[maybe_unused]] const LogicalDeviceView logicalDevice)
	{
		vkUnmapMemory(logicalDevice, m_memoryAllocation.m_memory);
	}

	void BufferVulkan::SetDebugNameDeviceBuffer(
		[[maybe_unused]] const LogicalDevice& logicalDevice, [[maybe_unused]] const ConstZeroTerminatedStringView name
	)
	{
		const VkDebugUtilsObjectNameInfoEXT debugInfo{
			VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
			nullptr,
			VK_OBJECT_TYPE_BUFFER,
			reinterpret_cast<uint64_t>((VkBuffer)m_buffer),
			name
		};

#if PLATFORM_APPLE
		vkSetDebugUtilsObjectNameEXT(logicalDevice, &debugInfo);
#else
		const PFN_vkSetDebugUtilsObjectNameEXT setDebugUtilsObjectNameEXT =
			reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(logicalDevice.GetSetDebugUtilsObjectNameEXT());
		if (setDebugUtilsObjectNameEXT != nullptr)
		{
			setDebugUtilsObjectNameEXT(logicalDevice, &debugInfo);
		}
#endif
	}
#endif
}
