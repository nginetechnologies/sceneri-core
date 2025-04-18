#pragma once

#include <Common/EnumFlagOperators.h>

namespace ngine::Input
{
	enum class MouseButton : uint8
	{
		Left = 1 << 0,
		Middle = 1 << 1,
		Right = 1 << 2,
		Extra1 = 1 << 3,
		Extra2 = 1 << 4,
		Count = 5
	};

	ENUM_FLAG_OPERATORS(MouseButton);
}
