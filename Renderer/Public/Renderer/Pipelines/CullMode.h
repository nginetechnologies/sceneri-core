#pragma once

#include <Common/EnumFlagOperators.h>

namespace ngine::Rendering
{
	enum class CullMode : uint8
	{
		None = 0,
		Front = 1 << 0,
		Back = 1 << 1,
		FrontAndBack = Front | Back
	};
	ENUM_FLAG_OPERATORS(CullMode);

	enum class WindingOrder : uint8
	{
		CounterClockwise,
		Clockwise
	};
}
