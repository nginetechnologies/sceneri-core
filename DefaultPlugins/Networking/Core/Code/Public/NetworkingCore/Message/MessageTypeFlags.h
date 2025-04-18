#pragma once

#include <Common/EnumFlagOperators.h>

namespace ngine::Network
{
	enum class MessageTypeFlags : uint16
	{
		FromHost = 1 << 0,
		ToHost = 1 << 1,
		FromClient = 1 << 2,
		ToClient = 1 << 3,
		ToAllClients = 1 << 4,
		//! Whether this message belongs to an object and should be called as a member function
		IsObjectFunction = 1 << 5,
		//! Whether this message belongs to a component
		IsComponentFunction = 1 << 6,
		//! Whether this message belongs to a datacomponent
		IsDataComponentFunction = 1 << 7,
		IsStreamedPropertyFunction = 1 << 8,
		AllowClientToHostWithoutAuthority = 1 << 9,
		//! Whether the compressed data size can differ at runtime
		HasDynamicCompressedDataSize = 1 << 10,
		HostToClient = FromHost | ToClient,
		HostToAllClients = FromHost | ToAllClients,
		ClientToHost = FromClient | ToHost,
		ClientToClient = FromClient | ToClient,
		ClientToAllClients = FromClient | ToAllClients,
		FromMask = FromHost | FromClient,
		ToMask = ToHost | ToClient | ToAllClients,
		ValidationMask = FromMask | ToMask
	};
	ENUM_FLAG_OPERATORS(MessageTypeFlags);
}
