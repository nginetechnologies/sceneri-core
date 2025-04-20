#pragma once

#include "Anchoring.h"
#include <Common/Math/Primitives/Rectangle.h>

namespace ngine::Widgets
{
	[[nodiscard]] inline Math::Rectanglei
	CalculateOffsetAnchoredArea(const Anchoring anchoring, Math::Rectanglei anchoredArea, const Math::Vector2i offset)
	{
		const Math::TVector2<Anchoring> anchoringAxis = {
			anchoring & ~(Anchoring::Top | Anchoring::Bottom | Anchoring::VerticalCenter),
			anchoring & ~(Anchoring::Left | Anchoring::Right | Anchoring::HorizontalCenter)
		};

		switch (anchoringAxis.x)
		{
			case Anchoring::Left:
				anchoredArea += Math::Vector2i{offset.x, 0};
				break;
			case Anchoring::Right:
				anchoredArea -= Math::Vector2i{offset.x, 0};
				break;
			case Anchoring::HorizontalCenter:
				anchoredArea += Math::Vector2i{offset.x / 2, 0};
				anchoredArea -= Math::Vector2i{offset.x / 2, 0};
				break;
			default:
				ExpectUnreachable();
		}

		switch (anchoringAxis.y)
		{
			case Anchoring::Top:
				anchoredArea += Math::Vector2i{0, offset.y};
				break;
			case Anchoring::Bottom:
				anchoredArea -= Math::Vector2i{0, offset.y};
				break;
			case Anchoring::VerticalCenter:
				anchoredArea += Math::Vector2i{0, offset.y / 2};
				anchoredArea -= Math::Vector2i{0, offset.y / 2};
				break;
			default:
				ExpectUnreachable();
		}

		return anchoredArea;
	}
}
