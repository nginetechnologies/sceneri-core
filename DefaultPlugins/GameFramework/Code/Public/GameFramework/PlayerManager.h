#pragma once

#include <GameFramework/PlayerInfo.h>
#include <GameFramework/VirtualController/VirtualControllerManager.h>

#include <Common/Storage/AtomicIdentifierMask.h>
#include <Common/Storage/IdentifierArray.h>
#include <Common/Memory/Optional.h>
#include <Common/Memory/Containers/Vector.h>

#include <NetworkingCore/Client/ClientIdentifier.h>

namespace ngine::GameFramework
{
	using ClientIdentifier = Network::ClientIdentifier;

	struct PlayerManager
	{
		void AddPlayer(const ClientIdentifier playerIdentifier, const Optional<Rendering::SceneView*> pSceneView);
		void RemovePlayer(const ClientIdentifier playerIdentifier);

		[[nodiscard]] Optional<const PlayerInfo*> FindPlayerInfo(const ClientIdentifier playerIdentifier) const
		{
			Assert(playerIdentifier.IsValid());
			if (playerIdentifier.IsValid())
			{
				if (m_activePlayersBitset.IsSet(playerIdentifier))
				{
					return &m_playerInfos[playerIdentifier];
				}
			}

			return Invalid;
		}

		template<typename CallbackType>
		void IterateActivePlayers(CallbackType&& callback)
		{
			auto iterator = m_activePlayersBitset.GetSetBitsIterator(0, ClientIdentifier::MaximumCount);
			for (auto activeBit : iterator)
			{
				auto index = ClientIdentifier::MakeFromValidIndex(activeBit);
				Memory::CallbackResult result = callback(&m_playerInfos[index]);
				if (result == Memory::CallbackResult::Break)
				{
					break;
				}
			}
		}

		[[nodiscard]] Optional<PlayerInfo*> FindPlayerInfo(const ClientIdentifier playerIdentifier)
		{
			Assert(playerIdentifier.IsValid());
			if (playerIdentifier.IsValid())
			{
				if (m_activePlayersBitset.IsSet(playerIdentifier))
				{
					return &m_playerInfos[playerIdentifier];
				}
			}

			return Invalid;
		}
	private:
		Threading::AtomicIdentifierMask<ClientIdentifier> m_activePlayersBitset;
		TIdentifierArray<PlayerInfo, ClientIdentifier> m_playerInfos;
		VirtualControllerManager m_virtualControllerManager;
	};
}
