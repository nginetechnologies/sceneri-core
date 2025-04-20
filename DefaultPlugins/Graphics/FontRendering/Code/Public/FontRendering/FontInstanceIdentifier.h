#pragma once

#include <Common/Storage/ForwardDeclarations/Identifier.h>

namespace ngine::Font
{
	// A font instance is a font with a specific combination of size, character set etc.
	// One font instance will match a rendered FontAtlas in order to reuse a render target
	using InstanceIdentifier = TIdentifier<uint32, 16>;
}
