#pragma once

#include <Common/Memory/ForwardDeclarations/Variant.h>
#include <Common/Memory/Containers/ForwardDeclarations/String.h>
#include <Common/Memory/Containers/ForwardDeclarations/StringView.h>
#include <Common/Math/ForwardDeclarations/Color.h>
#include <Common/Math/Primitives/ForwardDeclarations/Spline.h>
#include <Common/Math/ForwardDeclarations/Vector2.h>
#include <Common/IO/ForwardDeclarations/Path.h>
#include <Common/IO/ForwardDeclarations/PathView.h>
#include <Common/IO/ForwardDeclarations/URI.h>
#include <Common/IO/ForwardDeclarations/URIView.h>

#include <Engine/Tag/TagIdentifier.h>
#include <Engine/Asset/Identifier.h>
#include <Engine/Entity/ComponentTypeIdentifier.h>

#include <Engine/Entity/ForwardDeclarations/ComponentSoftReference.h>

#include "../GenericIdentifier.h"

namespace ngine
{
	struct Guid;

	namespace Tag
	{
		struct Mask;
	}
	namespace Asset
	{
		struct Guid;
		struct Mask;
	}
	namespace Math
	{
		struct LinearGradient;
	}
}

namespace ngine::DataSource
{
	using PropertyValue = Variant<
		String,
		ConstStringView,
		UnicodeString,
		ConstUnicodeStringView,
		IO::Path,
		IO::PathView,
		IO::URI,
		IO::URIView,
		Guid,
		Asset::Guid,
		Asset::Identifier,
		Asset::Mask,
		GenericDataIdentifier,
		Tag::Identifier,
		Tag::Mask,
		Entity::ComponentTypeIdentifier,
		Entity::ComponentSoftReference,
		Entity::ComponentSoftReferences,
		Math::Color,
		Math::LinearGradient,
		Math::Spline2f,
		uint32,
		int32,
		float,
		bool>;
}
