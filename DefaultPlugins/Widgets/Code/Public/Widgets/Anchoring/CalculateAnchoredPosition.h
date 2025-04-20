#pragma once

#include "Anchoring.h"
#include <Common/Math/Primitives/Rectangle.h>

namespace ngine::Widgets
{
	[[nodiscard]] inline Math::Vector2i
	CalculateAnchoredPosition(const Math::Rectanglei anchorArea, const Math::Vector2i size, const Anchoring anchoring)
	{
		Math::Vector2i result;
		const Math::TVector2<Anchoring> anchoringAxis = {
			anchoring & ~(Anchoring::Top | Anchoring::Bottom | Anchoring::VerticalCenter),
			anchoring & ~(Anchoring::Left | Anchoring::Right | Anchoring::HorizontalCenter)
		};

		switch (anchoringAxis.x)
		{
			case Anchoring::Left:
				result.x = anchorArea.GetPosition().x;
				break;
			case Anchoring::Right:
				result.x = anchorArea.GetEndPosition().x - size.x;
				break;
			case Anchoring::HorizontalCenter:
				result.x = anchorArea.GetPosition().x + anchorArea.GetSize().x / 2 - size.x / 2;
				break;
			default:
				ExpectUnreachable();
		}

		switch (anchoringAxis.y)
		{
			case Anchoring::Top:
				result.y = anchorArea.GetPosition().y;
				break;
			case Anchoring::Bottom:
				result.y = anchorArea.GetEndPosition().y - size.y;
				break;
			case Anchoring::VerticalCenter:
				result.y = anchorArea.GetPosition().y + anchorArea.GetSize().y / 2 - size.y / 2;
				break;
			default:
				ExpectUnreachable();
		}

		return result;
	}
}
