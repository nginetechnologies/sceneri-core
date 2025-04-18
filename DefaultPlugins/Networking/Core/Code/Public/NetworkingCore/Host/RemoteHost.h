#pragma once

#include <NetworkingCore/RemotePeer.h>

namespace ngine::Network
{
	struct RemoteHost final : public RemotePeer
	{
		using RemotePeer::RemotePeer;
	};
}
