#pragma once

#include <Common/EnumFlagOperators.h>

namespace ngine::Rendering
{
	enum class MemoryFlags : uint8
	{
		DeviceLocal = 1 << 0,
		HostVisible = 1 << 1,
		HostCoherent = 1 << 2,
		HostCached = 1 << 3,
		LazilyAllocated = 1 << 4,
		Protected = 1 << 5,
		AllocateDeviceAddress = 1 << 6
	};

	ENUM_FLAG_OPERATORS(MemoryFlags);
}
