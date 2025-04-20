#pragma once

namespace ngine::Widgets
{
	enum class Anchoring : uint8
	{
		Top = 1 << 0,
		Left = 1 << 1,
		Right = 1 << 2,
		Bottom = 1 << 3,
		HorizontalCenter = 1 << 4,
		VerticalCenter = 1 << 5,
		TopLeft = Top | Left,
		TopRight = Top | Right,
		BottomLeft = Bottom | Left,
		BottomRight = Bottom | Right,
		LeftVerticalCenter = Left | VerticalCenter,
		TopHorizontalCenter = Top | HorizontalCenter,
	};

	ENUM_FLAG_OPERATORS(Anchoring);
}
