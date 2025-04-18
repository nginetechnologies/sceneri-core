#pragma once

#include <Common/Math/Vector2.h>

namespace ngine
{
	struct ScreenCoordinate;

	//! Coordinate relative to a window's origin (upper left)
	struct WindowCoordinate : public Math::Vector2i
	{
		using BaseType = Math::Vector2i;
		using BaseType::BaseType;
		WindowCoordinate(const Math::Vector2i coordinate)
			: BaseType(coordinate)
		{
		}
		WindowCoordinate(const ScreenCoordinate) = delete;
		WindowCoordinate& operator=(const ScreenCoordinate) = delete;
	};
}
