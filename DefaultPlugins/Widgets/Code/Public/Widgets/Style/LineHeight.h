#pragma once

#include <Widgets/Style/SizeAxis.h>

namespace ngine::Widgets::Style
{
	struct LineHeight
	{
		// A normal line height. This is default
		// Normal represents the default multiplier
		static constexpr Math::Ratiof DefaultFontRatio = 120_percent;

		constexpr Height() = default;

		constexpr explicit Height(const Point value)
			: m_value(value)
		{
		}

		constexpr explicit Height(const Math::Ratiof value)
			: m_value(value)
		{
		}

		[[nodiscard]] bool operator==(const Height& other) const
		{
			return m_value == other.m_value;
		}
		[[nodiscard]] bool operator!=(const Height& other) const
		{
			return m_value != other.m_value;
		}

		[[nodiscard]] Optional<const Point*> GetAsLength() const
		{
			return m_value.Get<Point>();
		}

		[[nodiscard]] Optional<Math::Ratiof> GetAsMultiplier() const
		{
			if (m_value.Is<Math::Ratiof>())
			{
				return m_value.GetExpected<Math::Ratiof>();
			}

			return InvalidType::Invalid;
		}
	protected:
		Variant<Point, Math::Ratiof> m_value{DefaultFontRatio};
	};
}
