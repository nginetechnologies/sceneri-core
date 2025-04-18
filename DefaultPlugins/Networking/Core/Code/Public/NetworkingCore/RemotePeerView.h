#pragma once

#include <Common/Platform/TrivialABI.h>

struct _ENetPeer;
typedef _ENetPeer ENetPeer;

namespace ngine::Network
{
	struct LocalPeer;
	struct LocalClient;

	struct TRIVIAL_ABI RemotePeerView
	{
		RemotePeerView() = default;

		[[nodiscard]] bool IsValid() const
		{
			return m_pNetPeer != nullptr;
		}
		[[nodiscard]] bool operator==(const RemotePeerView other) const
		{
			return m_pNetPeer == other.m_pNetPeer;
		}
	protected:
		friend LocalPeer;
		friend LocalClient;
		RemotePeerView(ENetPeer* pNetPeer)
			: m_pNetPeer(pNetPeer)
		{
		}
	protected:
		ENetPeer* m_pNetPeer = nullptr;
	};
}
