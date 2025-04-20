#pragma once

#include <Common/Math/Vector2.h>

namespace ngine
{
	struct ScreenCoordinate;
	struct WindowCoordinate;
}

namespace ngine::Widgets
{
	//! Coordinate of a widget relative to its parent widget
	struct LocalWidgetCoordinate : public Math::Vector2i
	{
		using BaseType = Math::Vector2i;
		using BaseType::BaseType;
		LocalWidgetCoordinate(const Math::Vector2i coordinate)
			: BaseType(coordinate)
		{
		}
		LocalWidgetCoordinate(const WindowCoordinate) = delete;
		LocalWidgetCoordinate& operator=(const WindowCoordinate) = delete;
		LocalWidgetCoordinate(const ScreenCoordinate) = delete;
		LocalWidgetCoordinate& operator=(const ScreenCoordinate) = delete;
	};
}
