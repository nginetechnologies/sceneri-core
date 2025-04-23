#include <Components/Player/Player.h>

#include <Renderer/Scene/SceneView.h>
#include <Common/System/Query.h>
#include <Engine/Entity/InputComponent.h>

#include <GameFramework/PlayerInfo.h>
#include <GameFramework/Plugin.h>
#include <Engine/Entity/Data/Component.inl>

namespace ngine::GameFramework
{
	Player::Player(Initializer&& initializer)
		: m_clientIdentifier(initializer.m_clientIdentifier)
	{
	}

	Optional<const Rendering::SceneView*> Player::GetSceneView() const
	{
		const GameFramework::PlayerManager& playerManager = System::FindPlugin<GameFramework::Plugin>()->GetPlayerManager();
		if (Optional<const PlayerInfo*> pPlayerInfo = playerManager.FindPlayerInfo(m_clientIdentifier))
		{
			return pPlayerInfo->GetSceneView();
		}

		return Invalid;
	}

	void Player::ChangeCamera(Entity::CameraComponent& camera)
	{
		GameFramework::PlayerManager& playerManager = System::FindPlugin<GameFramework::Plugin>()->GetPlayerManager();
		if (Optional<PlayerInfo*> pPlayerInfo = playerManager.FindPlayerInfo(m_clientIdentifier))
		{
			if (Optional<Rendering::SceneView*> pSceneView = pPlayerInfo->GetSceneView())
			{
				pSceneView->AssignCamera(camera);
			}
		}
	}

	void Player::AssignInput(Entity::InputComponent& input)
	{
		GameFramework::PlayerManager& playerManager = System::FindPlugin<GameFramework::Plugin>()->GetPlayerManager();
		if (Optional<PlayerInfo*> pPlayerInfo = playerManager.FindPlayerInfo(m_clientIdentifier))
		{
			if (Optional<Input::Monitor*> pInputMonitor = pPlayerInfo->GetInputMonitor())
			{
				input.AssignMonitor(static_cast<Input::ActionMonitor&>(*pInputMonitor));
			}
		}
	}

	void Player::UnassignInput(Entity::InputComponent& input)
	{
		GameFramework::PlayerManager& playerManager = System::FindPlugin<GameFramework::Plugin>()->GetPlayerManager();
		if (Optional<PlayerInfo*> pPlayerInfo = playerManager.FindPlayerInfo(m_clientIdentifier))
		{
			if (Optional<Input::Monitor*> pInputMonitor = pPlayerInfo->GetInputMonitor())
			{
				input.UnassignMonitor();
			}
		}
	}
}
