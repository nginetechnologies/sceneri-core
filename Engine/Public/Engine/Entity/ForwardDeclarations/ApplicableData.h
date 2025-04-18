#pragma once

#include <Engine/Entity/ForwardDeclarations/ComponentSoftReference.h>

#include <Common/Memory/ForwardDeclarations/Variant.h>
#include <Common/IO/ForwardDeclarations/Path.h>
#include <Common/IO/ForwardDeclarations/URI.h>

namespace ngine::Asset
{
	struct Reference;
	struct LibraryReference;
}

namespace ngine::Entity
{
	//! Data that can (potentially) be applied to components
	using ApplicableData = Variant<Asset::Reference, Asset::LibraryReference, Entity::ComponentSoftReference, IO::Path, IO::URI>;
}
