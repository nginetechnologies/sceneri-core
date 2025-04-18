#pragma once

#include <Common/EnumFlagOperators.h>
#include <Common/Math/CoreNumericTypes.h>

namespace ngine::Network
{
	enum class MessageFlags : uint8
	{
		//! Packet must be received by the target peer and resend attempts should be made until the packet is delivered
		Reliable = 1 << 0,
		//! Packet does not need to be received nor acknowledged by the target peer, and does not need to be sent in an ordered sequence. The
		//! absence of this flag or Reliable implies an unreliable sequenced package, aka a package that does not need to be acknowledged but is
		//! sent / received in order.
		UnreliableUnsequenced = 1 << 1,
	};
	ENUM_FLAG_OPERATORS(MessageFlags);
}
