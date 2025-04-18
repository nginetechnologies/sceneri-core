#pragma once

#include <Common/Storage/ForwardDeclarations/Identifier.h>

namespace ngine::Network
{
	//! Identifies an object that has been bound to the network, meaning it has a replicated equivalent on all clients and server that can be
	//! uniquely identified
	using BoundObjectIdentifier = TIdentifier<uint32, 14>;
}
