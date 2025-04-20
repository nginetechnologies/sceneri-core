#pragma once

#include "ReferenceValue.h"
#include "PixelValue.h"
#include "ViewportRatio.h"
#include "Point.h"

#include <Engine/DataSource/DataSourcePropertyIdentifier.h>

#include <Common/Math/Ratio.h>
#include <Common/Memory/Variant.h>
#include <Common/Storage/Identifier.h>

namespace ngine::Widgets::Style
{
	enum class AutoType : uint8
	{
		Auto
	};
	inline static constexpr AutoType Auto = AutoType::Auto;

	//! Used to indicate that no maximum value was set, and that no clamping should be used.
	enum class NoMaximumType : uint8
	{
		NoMaximum
	};
	inline static constexpr NoMaximumType NoMaximum = NoMaximumType::NoMaximum;

	//! Used when size should be the same as the smallest child
	enum class MinContentSizeType : uint8
	{
		MinContent
	};
	inline static constexpr MinContentSizeType MinContentSize = MinContentSizeType::MinContent;

	//! Used when size should be the same as the largest child
	enum class MaxContentSizeType : uint8
	{
		MaxContent
	};
	inline static constexpr MaxContentSizeType MaxContentSize = MaxContentSizeType::MaxContent;

	//! Used when size should use the available space in the parent, but never more than the largest child
	enum class FitContentSizeType : uint8
	{
		FitContent
	};
	inline static constexpr FitContentSizeType FitContentSize = FitContentSizeType::FitContent;

	//! Used when size should use the screen's left safe area inset value
	enum class SafeAreaInsetLeftType : uint8
	{
		SafeAreaInsetLeft
	};
	inline static constexpr SafeAreaInsetLeftType SafeAreaInsetLeft = SafeAreaInsetLeftType::SafeAreaInsetLeft;

	//! Used when size should use the screen's right safe area inset value
	enum class SafeAreaInsetRightType : uint8
	{
		SafeAreaInsetRight
	};
	inline static constexpr SafeAreaInsetRightType SafeAreaInsetRight = SafeAreaInsetRightType::SafeAreaInsetRight;

	//! Used when size should use the screen's top safe area inset value
	enum class SafeAreaInsetTopType : uint8
	{
		SafeAreaInsetTop
	};
	inline static constexpr SafeAreaInsetTopType SafeAreaInsetTop = SafeAreaInsetTopType::SafeAreaInsetTop;

	//! Used when size should use the screen's bottom safe area inset value
	enum class SafeAreaInsetBottomType : uint8
	{
		SafeAreaInsetBottom
	};
	inline static constexpr SafeAreaInsetBottomType SafeAreaInsetBottom = SafeAreaInsetBottomType::SafeAreaInsetBottom;

	using DesiredSize = Variant<
		ReferencePixel,
		PhysicalSizePixel,
		Math::Ratiof,
		PixelValue,
		ViewportWidthRatio,
		ViewportHeightRatio,
		ViewportMinimumRatio,
		ViewportMaximumRatio,
		float,
		Font::Point,
		AutoType,
		NoMaximumType,
		MinContentSizeType,
		MaxContentSizeType,
		FitContentSizeType,
		SafeAreaInsetLeftType,
		SafeAreaInsetRightType,
		SafeAreaInsetTopType,
		SafeAreaInsetBottomType,
		ngine::DataSource::PropertyIdentifier>;

	static_assert(TypeTraits::IsTriviallyDestructible<DesiredSize>);
}
