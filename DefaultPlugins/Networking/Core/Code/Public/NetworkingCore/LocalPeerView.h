#pragma once

#include <Common/Platform/ForceInline.h>
#include <Common/Platform/TrivialABI.h>

struct _ENetHost;
typedef _ENetHost ENetHost;

namespace ngine::Network
{
	struct TRIVIAL_ABI LocalPeerView
	{
		LocalPeerView() = default;

		[[nodiscard]] bool IsValid() const
		{
			return m_pNetHost != nullptr;
		}
	protected:
		LocalPeerView(ENetHost* pNetHost)
			: m_pNetHost(pNetHost)
		{
		}
	protected:
		ENetHost* m_pNetHost = nullptr;
	};
}
