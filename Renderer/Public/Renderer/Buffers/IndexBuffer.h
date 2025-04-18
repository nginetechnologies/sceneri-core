#pragma once

#include <Renderer/Buffers/Buffer.h>

namespace ngine::Rendering
{
	struct IndexBuffer : public Buffer
	{
		IndexBuffer() = default;
		IndexBuffer(LogicalDevice& logicalDevice, const PhysicalDevice& physicalDevice, DeviceMemoryPool& memoryPool, const size size);
		IndexBuffer(const IndexBuffer&) = delete;
		IndexBuffer& operator=(const IndexBuffer&) = delete;
		IndexBuffer(IndexBuffer&&) = default;
		IndexBuffer& operator=(IndexBuffer&&) = default;
	};
}
