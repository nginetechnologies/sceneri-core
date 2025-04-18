#pragma once

#include <Common/Math/Primitives/RectangleEdges.h>

namespace ngine::Rendering
{
	struct ScreenProperties
	{
		float m_dotsPerInch;
		//! The pixel ratio / scale needed to convert from CSS coordinates to device pixels
		//! A value of 1 indicates a classic 96 DPI display.
		float m_devicePixelRatio;
		//! The safe area used to wrap around non-rectanglular screens
		Math::TRectangleEdges<float> m_safeArea;
		//! Screen dimensions
		Math::Vector2ui m_dimensions;
		uint16 m_maximumRefreshRate;
	};
}
