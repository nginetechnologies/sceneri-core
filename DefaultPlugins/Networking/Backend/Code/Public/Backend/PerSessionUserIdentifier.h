#pragma once

#include <Common/Storage/ForwardDeclarations/Identifier.h>

namespace ngine::Networking::Backend
{
	using PerSessionUserIdentifier = TIdentifier<uint32, 16>;
}
