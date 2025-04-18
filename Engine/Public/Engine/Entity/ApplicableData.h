#pragma once

#include <Engine/Entity/ComponentSoftReference.h>

#include <Common/Memory/Variant.h>
#include <Common/Asset/Reference.h>
#include <Common/IO/Path.h>
#include <Common/IO/URI.h>

namespace ngine::Entity
{
	//! Data that can (potentially) be applied to components
	using ApplicableData = Variant<Asset::Reference, Asset::LibraryReference, Entity::ComponentSoftReference, IO::Path, IO::URI>;
}
