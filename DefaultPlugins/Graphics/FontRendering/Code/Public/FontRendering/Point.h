#pragma once

#include <Common/Serialization/ForwardDeclarations/Reader.h>
#include <Common/Serialization/ForwardDeclarations/Writer.h>
#include <Common/Math/Ceil.h>
#include <Common/Math/Abs.h>
#include <Common/Math/NumericLimits.h>
#include <Common/Guid.h>
#include <Common/Reflection/Type.h>

namespace ngine::Font
{
	// Represents the common format of 'pt', where 1 pt = 72 inches
	struct Point
	{
		inline static constexpr Guid TypeGuid = "{7182265C-497F-4468-8E8C-FADCFAC62892}"_guid;

		constexpr Point()
			: m_value(0.f)
		{
		}
		[[nodiscard]] static constexpr Point FromInches(const float value)
		{
			return Point(value);
		}
		[[nodiscard]] static constexpr Point FromValue(const float value)
		{
			return Point(value / 72.f);
		}
		[[nodiscard]] static constexpr Point FromPixel(const float value)
		{
			return Point::FromValue(value * (72.0f / 96.0f));
		}

		bool Serialize(const Serialization::Reader);
		bool Serialize(Serialization::Writer) const;

		[[nodiscard]] float GetPixels() const
		{
			// Converts to a standard web-style "pixel" where 1 px = 96 inches
			return GetPoints() * (96.f / 72.f);
		}

		[[nodiscard]] float GetPoints() const
		{
			return m_value * 72;
		}

		[[nodiscard]] float GetPoints(const float dotsPerInch) const
		{
			return GetInches() * dotsPerInch;
		}

		[[nodiscard]] float GetInches() const
		{
			return m_value;
		}

		[[nodiscard]] int32 GetPixels(const float dotsPerInch) const
		{
			return (int32)Math::Ceil(GetPoints(dotsPerInch));
		}

		[[nodiscard]] bool operator==(const Point& other) const
		{
			return Math::Abs(m_value - other.m_value) <= Math::NumericLimits<float>::Epsilon;
		}
		[[nodiscard]] bool operator!=(const Point& other) const
		{
			return !operator==(other);
		}

		[[nodiscard]] PURE_STATICS Point operator-() const
		{
			return Point(-m_value);
		}
	protected:
		constexpr Point(const float value)
			: m_value{value}
		{
		}

		float m_value;
	};

	namespace Literals
	{
		constexpr Point operator""_pt(unsigned long long value) noexcept
		{
			return Point::FromValue(static_cast<float>(value));
		}

		constexpr Point operator""_pt(long double value) noexcept
		{
			return Point::FromValue(static_cast<float>(value));
		}
	}

	using namespace ngine::Font::Literals;
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<ngine::Font::Point>
	{
		inline static constexpr auto Type =
			Reflection::Reflect<ngine::Font::Point>(ngine::Font::Point::TypeGuid, MAKE_UNICODE_LITERAL("Font Point"));
	};
}
