#pragma once

namespace ngine::Widgets
{
	enum class ContentAreaChangeFlags : uint8
	{
		PositionXChanged = 1 << 0,
		PositionYChanged = 1 << 1,
		SizeXChanged = 1 << 2,
		SizeYChanged = 1 << 3,
		PositionChanged = PositionXChanged | PositionYChanged,
		SizeChanged = SizeXChanged | SizeYChanged
	};

	ENUM_FLAG_OPERATORS(ContentAreaChangeFlags);
}
