#pragma once

#include <Common/Math/CoreNumericTypes.h>
#include <Common/EnumFlagOperators.h>

namespace ngine::Input
{
	enum class KeyboardModifier : uint16
	{
		LeftShift = 1 << 0,
		RightShift = 1 << 1,
		LeftControl = 1 << 2,
		RightControl = 1 << 3,
		LeftAlt = 1 << 4,
		RightAlt = 1 << 5,
		LeftCommand = 1 << 6,
		RightCommand = 1 << 7,
		Capital = 1 << 8,
	};

	ENUM_FLAG_OPERATORS(KeyboardModifier);
}
