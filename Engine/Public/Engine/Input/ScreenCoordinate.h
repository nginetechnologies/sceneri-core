#pragma once

#include <Common/Math/Vector2.h>

namespace ngine
{
	struct WindowCoordinate;

	struct ScreenCoordinate : public Math::Vector2i
	{
		using BaseType = Math::Vector2i;
		using BaseType::BaseType;
		ScreenCoordinate(const Math::Vector2i coordinate)
			: BaseType(coordinate)
		{
		}
		ScreenCoordinate(const WindowCoordinate) = delete;
		ScreenCoordinate& operator=(const WindowCoordinate) = delete;
	};
}
