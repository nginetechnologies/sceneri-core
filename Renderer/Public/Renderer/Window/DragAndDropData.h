#pragma once

#include <Common/Memory/ForwardDeclarations/Variant.h>
#include <Common/Memory/Containers/ForwardDeclarations/String.h>
#include <Common/IO/ForwardDeclarations/Path.h>
#include <Common/IO/ForwardDeclarations/PathView.h>
#include <Common/Asset/Picker.h>

#include <Engine/Entity/ForwardDeclarations/ComponentSoftReference.h>

namespace ngine::Widgets
{
	using DragAndDropData = Variant<IO::Path, String, Asset::Reference, Asset::LibraryReference, Entity::ComponentSoftReference>;
	using DragAndDropDataView = Variant<IO::PathView, StringView, Asset::Reference, Asset::LibraryReference, Entity::ComponentSoftReference>;
}
