#pragma once

#include <Renderer/Buffers/Buffer.h>

namespace ngine::Rendering
{
	struct VertexBuffer : public Buffer
	{
		VertexBuffer() = default;
		VertexBuffer(
			LogicalDevice& logicalDevice,
			const PhysicalDevice& physicalDevice,
			DeviceMemoryPool& memoryPool,
			const size size,
			const EnumFlags<UsageFlags> usageFlags = {},
			const bool allowCpuAccess = false
		);
		VertexBuffer(const VertexBuffer&) = delete;
		VertexBuffer& operator=(const VertexBuffer&) = delete;
		VertexBuffer(VertexBuffer&&) = default;
		VertexBuffer& operator=(VertexBuffer&&) = default;
	};
}
