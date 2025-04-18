#pragma once

#include <Common/EnumFlagOperators.h>

namespace ngine::Rendering
{
	enum class ColorChannel : uint32
	{
		None = 0,
		Red = 1 << 0,
		Green = 1 << 1,
		Blue = 1 << 2,
		Alpha = 1 << 3,
		RG = Red | Green,
		RGB = Red | Green | Blue,
		RGBA = Red | Green | Blue | Alpha,
		All = RGBA
	};
	ENUM_FLAG_OPERATORS(ColorChannel);
}
