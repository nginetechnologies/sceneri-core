#pragma once

#include "SizeAxis.h"
#include <Common/Math/Vector2/Floor.h>

namespace ngine::Widgets::Style
{
	struct Size
	{
		Size() = default;
		Size(const Math::ZeroType)
			: x(Math::Zero)
			, y(Math::Zero)
		{
		}
		Size(const InvalidType)
			: x(Invalid)
			, y(Invalid)
		{
		}
		Size(const AutoType)
			: x(Auto)
			, y(Auto)
		{
		}
		template<
			typename WidthType,
			typename HeightType,
			typename =
				EnableIf<TypeTraits::HasConstructor<SizeAxisExpression, WidthType> && TypeTraits::HasConstructor<SizeAxisExpression, HeightType>>>
		constexpr Size(const WidthType width, const HeightType height)
			: x(width)
			, y(height)
		{
		}
		constexpr Size(const ReferencePixelX width, const ReferencePixelY height)
			: Size(ReferencePixel(width), ReferencePixel(height))
		{
		}
		constexpr Size(const ReferencePixel value)
			: Size(value, value)
		{
		}
		constexpr Size(const ReferencePixelX width)
			: Size(ReferencePixel(width), Math::Zero)
		{
		}
		constexpr Size(const ReferencePixelY height)
			: Size(Math::Zero, ReferencePixel(height))
		{
		}
		Size(const SizeAxisExpression size)
			: x(size)
			, y(size)
		{
		}
		Size(const PixelValue pixelValueX, const PixelValue pixelValueY)
			: x(pixelValueX)
			, y(pixelValueY)
		{
		}

		[[nodiscard]] SizeAxisExpression& operator[](const uint8 index)
		{
			Expect(index < 2);
			return *(&x + index);
		}
		[[nodiscard]] const SizeAxisExpression& operator[](const uint8 index) const
		{
			Expect(index < 2);
			return *(&x + index);
		}

		[[nodiscard]] bool operator==(const Size& other) const
		{
			return x == other.x && y == other.y;
		}
		[[nodiscard]] bool operator!=(const Size& other) const
		{
			return x != other.x || y != other.y;
		}

		SizeAxisExpression x;
		SizeAxisExpression y;

		[[nodiscard]] Math::Vector2f GetPoints(const Math::Vector2f owningSize, const Rendering::ScreenProperties screenProperties) const
		{
			return {x.GetPoint(owningSize.x, screenProperties), y.GetPoint(owningSize.y, screenProperties)};
		}

		[[nodiscard]] Math::Vector2i Get(const Math::Vector2i owningSize, const Rendering::ScreenProperties screenProperties) const
		{
			return (Math::Vector2i)Math::Floor(GetPoints((Math::Vector2f)owningSize, screenProperties));
		}

		[[nodiscard]] int32 GetX(const Math::Vector2i owningSize, const Rendering::ScreenProperties screenProperties) const
		{
			return x.Get(owningSize.x, screenProperties);
		}

		[[nodiscard]] float GetPointX(const Math::Vector2f owningSize, const Rendering::ScreenProperties screenProperties) const
		{
			return x.GetPoint(owningSize.x, screenProperties);
		}

		[[nodiscard]] int32 GetY(const Math::Vector2i owningSize, const Rendering::ScreenProperties screenProperties) const
		{
			return y.Get(owningSize.y, screenProperties);
		}

		[[nodiscard]] float GetPointY(const Math::Vector2f owningSize, const Rendering::ScreenProperties screenProperties) const
		{
			return y.GetPoint(owningSize.y, screenProperties);
		}

		[[nodiscard]] Math::Vector2f GetPoints(
			const Math::Vector2f owningSize,
			const Math::Vector2i smallestChildSize,
			const Math::Vector2i largestChildSize,
			const Rendering::ScreenProperties screenProperties
		) const
		{
			return {
				x.GetPoint(owningSize.x, smallestChildSize.x, largestChildSize.x, screenProperties),
				y.GetPoint(owningSize.y, smallestChildSize.y, largestChildSize.y, screenProperties)
			};
		}

		[[nodiscard]] Math::Vector2i Get(
			const Math::Vector2i owningSize,
			const Math::Vector2i smallestChildSize,
			const Math::Vector2i largestChildSize,
			const Rendering::ScreenProperties screenProperties
		) const
		{
			return (Math::Vector2i)Math::Floor(GetPoints((Math::Vector2f)owningSize, smallestChildSize, largestChildSize, screenProperties));
		}

		[[nodiscard]] int32 GetX(
			const Math::Vector2i owningSize,
			const Math::Vector2i smallestChildSize,
			const Math::Vector2i largestChildSize,
			const Rendering::ScreenProperties screenProperties
		) const
		{
			return x.Get(owningSize.x, smallestChildSize.x, largestChildSize.x, screenProperties);
		}

		[[nodiscard]] float GetPointX(
			const Math::Vector2f owningSize,
			const Math::Vector2i smallestChildSize,
			const Math::Vector2i largestChildSize,
			const Rendering::ScreenProperties screenProperties
		) const
		{
			return x.GetPoint(owningSize.x, smallestChildSize.x, largestChildSize.x, screenProperties);
		}

		[[nodiscard]] int32 GetY(
			const Math::Vector2i owningSize,
			const Math::Vector2i smallestChildSize,
			const Math::Vector2i largestChildSize,
			const Rendering::ScreenProperties screenProperties
		) const
		{
			return y.Get(owningSize.y, smallestChildSize.y, largestChildSize.y, screenProperties);
		}

		[[nodiscard]] float GetPointY(
			const Math::Vector2f owningSize,
			const Math::Vector2i smallestChildSize,
			const Math::Vector2i largestChildSize,
			const Rendering::ScreenProperties screenProperties
		) const
		{
			return y.GetPoint(owningSize.y, smallestChildSize.y, largestChildSize.y, screenProperties);
		}

		bool Serialize(const Serialization::Reader reader);
		bool Serialize(Serialization::Writer writer) const;
	};
}
