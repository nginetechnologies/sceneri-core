#pragma once

#include <Renderer/Buffers/Buffer.h>

namespace ngine::Rendering
{
	struct StorageBuffer : public Buffer
	{
		enum class Flags : uint8
		{
			TransferSource = 1 << 0,
			TransferDestination = 1 << 1
		};

		StorageBuffer() = default;
		StorageBuffer(
			LogicalDevice& logicalDevice,
			const PhysicalDevice& physicalDevice,
			DeviceMemoryPool& memoryPool,
			const size size,
			const EnumFlags<Flags> flags = Flags::TransferDestination
		);
		StorageBuffer(const StorageBuffer&) = delete;
		StorageBuffer& operator=(const StorageBuffer&) = delete;
		StorageBuffer(StorageBuffer&&) = default;
		StorageBuffer& operator=(StorageBuffer&&) = default;
	};

	ENUM_FLAG_OPERATORS(StorageBuffer::Flags);
}
