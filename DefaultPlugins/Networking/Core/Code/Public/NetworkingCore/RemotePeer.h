#pragma once

#include "RemotePeerView.h"
#include "Message/MessageFlags.h"
#include "Channel.h"

#include <Common/Memory/Containers/ByteView.h>
#include <Common/EnumFlags.h>

namespace ngine::Network
{
	struct LocalClient;
	struct LocalHost;
	struct LocalPeer;

	struct EncodedMessageBuffer;

	struct RemotePeer : public RemotePeerView
	{
		using RemotePeerView::RemotePeerView;
	protected:
		friend LocalClient;
		friend LocalHost;
		friend LocalPeer;

		//! Starts disconnecting the remote peer from the local host
		//! This is not immediate, as it will start negotiating a disconnect with the remote peer
		void Disconnect(const uint32 disconnectUserData);

		//! Force disconnect the remote peer from the local host
		void ForceDisconnect();

		//! Sends a message to this peer
		bool SendMessageTo(
			EncodedMessageBuffer&& encodedMessageBuffer,
			const Channel channel,
			const EnumFlags<MessageFlags> messageFlags = MessageFlags::Reliable
		);
	};
}
