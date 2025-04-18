#pragma once

#include <Common/Storage/ForwardDeclarations/Identifier.h>

namespace ngine::Tag
{
	//! A unique runtime identifier for a tag
	//! A tag can be applied to assets and components in order to support filtering
	//! For example, a category could be implemented as a tag and would allow fetching all assets or components matching a specific category /
	//! tag.
	using Identifier = TIdentifier<uint32, 15>;
}
