#pragma once

#include <Common/EnumFlagOperators.h>

namespace ngine::Networking::Backend
{
	enum class RequestFlags : uint8
	{
		HighPriority = 1 << 0
	};
	ENUM_FLAG_OPERATORS(RequestFlags);
}
