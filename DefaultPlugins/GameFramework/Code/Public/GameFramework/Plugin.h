#pragma once

#include <GameFramework/PlayerManager.h>

#include <Common/Plugin/Plugin.h>
#include <Common/Guid.h>

namespace ngine::GameFramework
{
	struct Plugin : public ngine::Plugin
	{
		inline static constexpr Guid Guid = "EAF8C037-4ADF-4A97-99FD-915341D1854C"_guid;

		Plugin(Application&)
		{
		}
		virtual ~Plugin() = default;

		// IPlugin
		virtual void OnLoaded(Application& application) override;
		// ~IPlugin

		[[nodiscard]] PlayerManager& GetPlayerManager()
		{
			return m_playerManager;
		}

		[[nodiscard]] const PlayerManager& GetPlayerManager() const
		{
			return m_playerManager;
		}
	private:
		PlayerManager m_playerManager;
	};
}
