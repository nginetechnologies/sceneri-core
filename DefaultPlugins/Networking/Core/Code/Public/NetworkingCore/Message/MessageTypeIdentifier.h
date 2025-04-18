#pragma once

#include <Common/Storage/ForwardDeclarations/Identifier.h>

namespace ngine::Network
{
	//! Identifier mapping a mesasge type to the peer local event identifier
	//! Must be 100% matching between clients and host
	using MessageTypeIdentifier = TIdentifier<uint32, 9>;
}
