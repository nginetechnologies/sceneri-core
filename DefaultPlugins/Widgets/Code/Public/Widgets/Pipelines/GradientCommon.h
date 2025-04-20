#pragma once

#include <Common/Math/LinearGradient.h>
#include <Common/Math/Vector2/Round.h>
#include <Common/Math/Rotation2D.h>

namespace ngine::Rendering
{
	inline void RotateGradient(
		const Math::LinearGradient& gradient, const FixedArrayView<Math::Color, 3> colors, const FixedArrayView<Math::Vector2f, 3> points
	)
	{
		// Rotate clock-wise
		Math::Anglef orientation = Math::PI2 - gradient.m_orientation;
		uint8 counter = 0;

		Math::Vector2f line(0.0, 1.0f);
		Math::WorldRotation2D rotation(orientation);
		line = Math::Round(rotation.TransformDirection(line));
		for (Math::LinearGradient::Color color :
		     gradient.m_colors.GetView().GetSubViewUpTo(Math::Min(gradient.m_colors.GetSize(), colors.GetSize(), points.GetSize())))
		{
			Math::Vector2f point(0.0f, color.m_stopPoint);
			point = line * point.GetLength();
			if (line.x < 0.0f)
			{
				point.x += 1.0f;
			}
			if (line.y < 0.0f)
			{
				point.y += 1.0f;
			}

			points[counter] = point;
			colors[counter] = color.m_color;
			counter++;
		}
	}
}
