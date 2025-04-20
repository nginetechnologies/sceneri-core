#pragma once

namespace ngine::Widgets
{
	struct PixelValue
	{
		int32 m_value;
		constexpr PixelValue(const int32 value)
			: m_value{value}
		{
		}

		[[nodiscard]] PixelValue operator-() const
		{
			return -m_value;
		}
	};
}
