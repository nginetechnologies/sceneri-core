#pragma once

#include <Common/Storage/ForwardDeclarations/Identifier.h>
#include <Common/Storage/ForwardDeclarations/AtomicIdentifierMask.h>

namespace ngine::Asset
{
	using Identifier = TIdentifier<uint32, 16>;
	using AtomicMask = Threading::AtomicIdentifierMask<Identifier>;
}
