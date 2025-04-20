#pragma once

#include <Common/EnumFlagOperators.h>
#include <Common/Math/CoreNumericTypes.h>

namespace ngine::Font
{
	enum class Modifier : uint8
	{
		None,
		Italic = 1 << 0
	};

	ENUM_FLAG_OPERATORS(Modifier);
}
