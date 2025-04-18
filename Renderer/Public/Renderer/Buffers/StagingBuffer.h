#pragma once

#include <Renderer/Buffers/Buffer.h>

namespace ngine::Rendering
{
	struct StagingBuffer : public Buffer
	{
		enum class Flags : uint8
		{
			TransferSource = 1 << 0,
			TransferDestination = 1 << 1
		};

		StagingBuffer() = default;
		StagingBuffer(
			LogicalDevice& logicalDevice,
			const PhysicalDevice& physicalDevice,
			DeviceMemoryPool& memoryPool,
			const size size,
			const EnumFlags<Flags> flags
		);
		StagingBuffer(const StagingBuffer&) = delete;
		StagingBuffer& operator=(const StagingBuffer&) = delete;
		StagingBuffer(StagingBuffer&&) = default;
		StagingBuffer& operator=(StagingBuffer&&) = default;
	};
	ENUM_FLAG_OPERATORS(StagingBuffer::Flags);
}
