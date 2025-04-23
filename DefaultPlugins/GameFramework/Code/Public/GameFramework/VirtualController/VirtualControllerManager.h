#pragma once

#include "VirtualController.h"

#include <NetworkingCore/Client/ClientIdentifier.h>

#include <Common/Storage/IdentifierArray.h>

namespace ngine::GameFramework
{
	struct PlayerInfo;
	using ClientIdentifier = Network::ClientIdentifier;

	class VirtualControllerManager
	{
	public:
		VirtualControllerManager();
		~VirtualControllerManager();

		VirtualControllerManager(VirtualControllerManager&& other) noexcept = delete;
		VirtualControllerManager(const VirtualControllerManager& other) = delete;

		VirtualControllerManager& operator=(const VirtualControllerManager& other) = delete;
		VirtualControllerManager& operator=(VirtualControllerManager&& other) = delete;

		void CreateController(const PlayerInfo& playerInfo);
		void DestroyController(const PlayerInfo& playerInfo);
	protected:
		void EnableVirtualControllers();
		void DisableVirtualControllers();
	private:
		TIdentifierArray<const PlayerInfo*, ClientIdentifier> m_playerInfos;
		TIdentifierArray<VirtualController, ClientIdentifier> m_controllers;
		bool m_isEnabled{false};
	};
}
