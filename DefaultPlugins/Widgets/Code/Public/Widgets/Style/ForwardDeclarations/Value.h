#pragma once

#include <Widgets/LayoutType.h>
#include <Widgets/PositionType.h>
#include <Widgets/Orientation.h>
#include <Widgets/Alignment.h>
#include <Widgets/OverflowType.h>
#include <Widgets/WordWrapType.h>
#include <Widgets/TextOverflowType.h>
#include <Widgets/WhiteSpaceType.h>
#include <Widgets/ElementSizingType.h>
#include <Widgets/WrapType.h>
#include <Widgets/Style/ForwardDeclarations/ReferenceValue.h>

#include <Engine/Entity/ForwardDeclarations/ComponentSoftReference.h>

#include <FontRendering/FontModifier.h>

#include <Common/ForwardDeclarations/EnumFlags.h>
#include <Common/Memory/ForwardDeclarations/Variant.h>
#include <Common/Memory/Containers/ForwardDeclarations/String.h>
#include <Common/Math/Primitives/ForwardDeclarations/RectangleCorners.h>
#include <Common/Math/Primitives/ForwardDeclarations/RectangleEdges.h>
#include <Common/Math/ForwardDeclarations/Color.h>
#include <Common/Math/Primitives/ForwardDeclarations/Spline.h>
#include <Common/Math/Ratio.h>

namespace ngine::Asset
{
	struct Guid;
}

namespace ngine::Font
{
	struct Weight;
	struct Height;
	struct Point;
}

namespace ngine::Math
{
	struct LinearGradient;
}

namespace ngine::Widgets::Style
{
	struct Size;
	struct SizeAxisExpression;
	using SizeAxisCorners = Math::TRectangleCorners<SizeAxisExpression>;
	using SizeAxisEdges = Math::TRectangleEdges<Style::SizeAxisExpression>;

	using EntryValue = Variant<
		UnicodeString,
		ReferencePixel,
		Size,
		SizeAxis,
		SizeAxisExpression,
		SizeAxisCorners,
		SizeAxisEdges,
		DataSource::PropertyIdentifier,
		Asset::Guid,
		Entity::ComponentSoftReference,
		Entity::ComponentSoftReference,
		PositionType,
		LayoutType,
		WordWrapType,
		TextOverflowType,
		WhiteSpaceType,
		Orientation,
		Alignment,
		OverflowType,
		ElementSizingType,
		WrapType,
		Font::Weight,
		EnumFlags<Font::Modifier>,
		Math::Color,
		Math::LinearGradient,
		Math::Spline2f,
		Math::Ratiof,
		int32,
		float,
		bool>;
}
