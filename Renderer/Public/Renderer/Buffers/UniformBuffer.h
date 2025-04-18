#pragma once

#include <Renderer/Buffers/Buffer.h>

namespace ngine::Rendering
{
	struct UniformBuffer : public Buffer
	{
		UniformBuffer() = default;
		UniformBuffer(LogicalDevice& logicalDevice, const PhysicalDevice& physicalDevice, DeviceMemoryPool& memoryPool, const size size);
		UniformBuffer(const UniformBuffer&) = delete;
		UniformBuffer& operator=(const UniformBuffer&) = delete;
		UniformBuffer(UniformBuffer&&) = default;
		UniformBuffer& operator=(UniformBuffer&&) = default;
	};
}
