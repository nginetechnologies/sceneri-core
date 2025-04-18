#pragma once

#include <Renderer/Buffers/BufferView.h>
#include <Renderer/Devices/MemoryFlags.h>
#include <Renderer/Buffers/DeviceMemory.h>
#include <Renderer/Devices/DeviceMemoryAllocation.h>

#include <Common/AtomicEnumFlags.h>
#include <Common/Memory/Containers/ForwardDeclarations/ZeroTerminatedStringView.h>
#include <Common/Memory/Containers/ByteView.h>
#include <Common/Function/ForwardDeclarations/Function.h>
#include <Common/Math/ForwardDeclarations/Range.h>

namespace ngine::Rendering
{
	struct LogicalDevice;
	struct LogicalDeviceView;
	struct PhysicalDevice;
	struct DeviceMemoryPool;
	struct CommandQueueView;

	struct BufferBase
	{
		enum class UsageFlags
		{
			// Maps to VkBufferUsageFlagBits
			TransferSource = 1 << 0,
			TransferDestination = 1 << 1,
			UniformBuffer = 1 << 4,
			StorageBuffer = 1 << 5,
			IndexBuffer = 1 << 6,
			VertexBuffer = 1 << 7,
			ShaderDeviceAddress = 0x00020000,
			AccelerationStructureBuildInputReadOnly = 0x00080000,
			AccelerationStructureStorage = 0x00100000,
		};

		enum class Flags : uint8
		{
			IsHostMappable = 1 << 0,
			IsHostMapping = 1 << 1,
			IsHostMapped = 1 << 2
		};

		enum class MapMemoryFlags : uint8
		{
			Read = 1 << 0,
			Write = 1 << 1,
			ReadWrite = Read | Write,
			//! Whether to keep the memory mapped
			//! Keep in mind that mapping can be an expensive operation, retain mapping if necessary
			KeepMapped = 1 << 2
		};

		enum class MapMemoryStatus : uint8
		{
			Success,
			MapFailed
		};

		//! Callback to be invoked when the buffer has finished mapping to host memory, or failed to.
		using MapMemoryAsyncCallback =
			Function<void(const MapMemoryStatus status, const ByteView data, const bool wasExecutedAsynchronously), 24>;

		BufferBase() = default;
		BufferBase(const size bufferSize, const EnumFlags<Flags> flags)
			: m_bufferSize(bufferSize)
			, m_flags(flags)
		{
		}

		BufferBase(const Buffer&) = delete;
		BufferBase& operator=(const Buffer&) = delete;

		BufferBase(BufferBase&& other) noexcept
			: m_buffer(other.m_buffer)
#if RENDERER_HAS_DEVICE_MEMORY
			, m_memoryAllocation(other.m_memoryAllocation)
#endif
			, m_bufferSize(other.m_bufferSize)
			, m_flags(other.m_flags)
		{
#if RENDERER_VULKAN || RENDERER_METAL || RENDERER_WEBGPU
			other.m_buffer.m_pBuffer = 0;
#endif

#if RENDERER_HAS_DEVICE_MEMORY
			other.m_memoryAllocation.m_memory = {};
#endif

			other.m_bufferSize = 0;
		}

		// what, how and why
		[[nodiscard]] operator BufferView() const
		{
			return m_buffer;
		}
	protected:
		BufferView m_buffer;
#if RENDERER_HAS_DEVICE_MEMORY
		DeviceMemoryAllocation m_memoryAllocation;
#endif
		size m_bufferSize = 0;
		AtomicEnumFlags<Flags> m_flags;
	};

	ENUM_FLAG_OPERATORS(BufferBase::UsageFlags);
	ENUM_FLAG_OPERATORS(BufferBase::Flags);
	ENUM_FLAG_OPERATORS(BufferBase::MapMemoryFlags);
}
