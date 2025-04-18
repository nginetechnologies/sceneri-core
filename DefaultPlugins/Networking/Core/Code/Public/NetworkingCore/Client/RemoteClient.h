#pragma once

#include <NetworkingCore/RemotePeer.h>
#include <NetworkingCore/Client/ClientIdentifier.h>

namespace ngine::Network
{
	struct LocalHost;

	struct RemoteClient final : public RemotePeer
	{
		using Identifier = ClientIdentifier;

		[[nodiscard]] Identifier GetIdentifier() const;
	protected:
		friend LocalHost;
		void OnConnected(const Identifier identifier);
		void OnDisconnected();
	};
}
