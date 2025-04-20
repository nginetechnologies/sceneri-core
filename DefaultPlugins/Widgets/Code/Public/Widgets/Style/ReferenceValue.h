#pragma once

#include <Common/Guid.h>
#include <Common/Serialization/ForwardDeclarations/Reader.h>
#include <Common/Serialization/ForwardDeclarations/Writer.h>
#include <Common/Math/Floor.h>

#include "ForwardDeclarations/ReferenceValue.h"

namespace ngine::Widgets
{
	template<uint8 ReferenceDPI>
	struct TReferenceValue
	{
		inline static constexpr Guid TypeGuid = "8893b113-8d80-45fe-91df-eaf055587d4f"_guid;

		float m_value;
		constexpr TReferenceValue()
			: m_value(0.f)
		{
		}
		constexpr TReferenceValue(const float value)
			: m_value{value}
		{
		}

		[[nodiscard]] TReferenceValue operator-() const
		{
			return -m_value;
		}

		[[nodiscard]] static constexpr TReferenceValue FromValue(const float value)
		{
			return TReferenceValue(value / ReferenceDPI);
		}
		[[nodiscard]] static constexpr TReferenceValue FromPoint(const float value, const float devicePixelRatio)
		{
			return (value / ReferenceDPI) / devicePixelRatio;
		}

		[[nodiscard]] constexpr float GetPoint() const
		{
			return m_value * ReferenceDPI;
		}
		[[nodiscard]] constexpr float GetPoint(const float devicePixelRatio) const
		{
			return m_value * ReferenceDPI * devicePixelRatio;
		}
		[[nodiscard]] constexpr int32 GetPixel() const
		{
			return (int32)Math::Floor(GetPoint());
		}
		[[nodiscard]] constexpr int32 GetPixel(const float devicePixelRatio) const
		{
			return (int32)Math::Floor(GetPoint(devicePixelRatio));
		}
		//! Gets the point fully relative to DPI, so that the displayed pixel will always be the exact same physical size
		//! /Useful for user interfaces that want to show physical elements such as rulers
		[[nodiscard]] constexpr float GetPhysicalPoint(const float dotsPerInch) const
		{
			return m_value * dotsPerInch;
		}
		//! Gets the pixel fully relative to DPI, so that the displayed pixel will always be the exact same physical size
		//! /Useful for user interfaces that want to show physical elements such as rulers
		[[nodiscard]] constexpr int32 GetPhysicalPixel(const float dotsPerInch) const
		{
			return (int32)Math::Floor(GetPhysicalPoint(dotsPerInch));
		}

		[[nodiscard]] bool operator==(const TReferenceValue& other) const
		{
			return m_value == other.m_value;
		}
		[[nodiscard]] bool operator!=(const TReferenceValue& other) const
		{
			return m_value != other.m_value;
		}

		bool Serialize(const Serialization::Reader);
		bool Serialize(Serialization::Writer) const;
	};

	extern template struct TReferenceValue<DpiReference>;

	struct ReferencePixelX : public ReferencePixel
	{
		constexpr ReferencePixelX(const ReferencePixel& value)
			: ReferencePixel(value)
		{
		}
		using ReferencePixel::ReferencePixel;
	};

	struct ReferencePixelY : public ReferencePixel
	{
		constexpr ReferencePixelY(const ReferencePixel& value)
			: ReferencePixel(value)
		{
		}
		using ReferencePixel::ReferencePixel;
	};

	//! A pixel that is fully relative to DPI, so that the displayed pixel will always be the exact same physical size
	//! Useful for user interfaces that want to show physical elements such as ruler
	struct PhysicalSizePixel : public TReferenceValue<DpiReference>
	{
		using BaseType = TReferenceValue<DpiReference>;
		using BaseType::BaseType;
		using BaseType::operator=;
	};

	namespace Literals
	{
		constexpr ReferencePixel operator""_px(unsigned long long value) noexcept
		{
			return ReferencePixel(static_cast<float>(value) / DpiReference);
		}

		constexpr ReferencePixel operator""_px(long double value) noexcept
		{
			return ReferencePixel(static_cast<float>(value / DpiReference));
		}

		constexpr PhysicalSizePixel operator""_dpi_px(unsigned long long value) noexcept
		{
			return PhysicalSizePixel(static_cast<float>(value) / DpiReference);
		}

		constexpr PhysicalSizePixel operator""_dpi_px(long double value) noexcept
		{
			return PhysicalSizePixel(static_cast<float>(value / DpiReference));
		}
	}

	using namespace Literals;
}
