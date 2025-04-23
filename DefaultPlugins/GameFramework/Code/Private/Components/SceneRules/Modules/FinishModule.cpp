#include "Components/SceneRules/Modules/FinishModule.h"
#include "Components/SceneRules/Modules/HUDModule.h"
#include "Components/SceneRules/Modules/SpawningModule.h"

#include <GameFramework/Components/SceneRules/SceneRules.h>
#include <GameFramework/Plugin.h>
#include <GameFramework/PlayerManager.h>
#include <GameFramework/PlayViewMode.h>

#include <Engine/Input/ActionMapBinding.inl>
#include <Engine/Entity/InputComponent.h>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/Component3D.h>
#include <Engine/Entity/RootSceneComponent.h>
#include <Engine/Entity/Component3D.inl>
#include <Engine/Entity/ComponentSoftReference.inl>
#include <Engine/Entity/CameraComponent.h>
#include <Engine/Entity/Data/Tags.h>
#include <Engine/Context/Utils.h>
#include <Engine/Context/EventManager.inl>

#include <Tags.h>
#include <Engine/Tag/TagRegistry.h>

#include <Renderer/Scene/SceneView.h>

#include <NetworkingCore/Components/ClientComponent.h>
#include <NetworkingCore/Components/LocalClientComponent.h>
#include <NetworkingCore/Components/BoundComponent.h>

#include <Common/Reflection/Registry.inl>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/Threading/Jobs/JobRunnerThread.h>

namespace ngine::GameFramework
{
	FinishModule::FinishModule(const FinishModule& templateComponent, const Cloner& cloner)
		: SceneRulesModule(templateComponent, cloner)
	{
	}

	FinishModule::FinishModule(const Deserializer& deserializer)
		: SceneRulesModule(deserializer)
	{
	}

	FinishModule::FinishModule(Initializer&& initializer)
		: SceneRulesModule(Forward<Initializer>(initializer))
	{
	}

	void FinishModule::OnGameplayStarted(SceneRules& sceneRules)
	{
		m_activePlayers = sceneRules.GetPlayersMask();

		if (Optional<SpawningModule*> pSpawningModule = sceneRules.FindDataComponentOfType<SpawningModule>())
		{
			pSpawningModule->OnPlayerSpawned.Add(*this, &FinishModule::OnPlayerSpawned);
		}
	}

	void FinishModule::OnGameplayStopped(SceneRules& sceneRules)
	{
		m_activePlayers.ClearAll();

		if (Optional<SpawningModule*> pSpawningModule = sceneRules.FindDataComponentOfType<SpawningModule>())
		{
			pSpawningModule->OnPlayerSpawned.Remove(this);
		}
	}

	void FinishModule::HostOnPlayerSceneLoaded(SceneRules& sceneRules, const ClientIdentifier playerIdentifier)
	{
		if (sceneRules.HasGameplayStarted())
		{
			m_activePlayers.Set(playerIdentifier);
		}
	}

	void FinishModule::HostOnPlayerLeft(SceneRules&, const ClientIdentifier playerIdentifier)
	{
		m_activePlayers.Clear(playerIdentifier);
	}

	void FinishModule::FinishAllRemainingPlayers(SceneRules& sceneRules, const GameRulesFinishResult finishResult)
	{
		for (const ClientIdentifier::IndexType clientIndex : m_activePlayers.GetSetBitsIterator())
		{
			const ClientIdentifier clientIdentifier = ClientIdentifier::MakeFromValidIndex(clientIndex);
			NotifyPlayerFinished(sceneRules, clientIdentifier, finishResult);
		}
	}

	void FinishModule::NotifyPlayerFinished(
		SceneRules& sceneRules, const ClientIdentifier playerIdentifier, const GameRulesFinishResult finishResult
	)
	{
		Assert(sceneRules.IsHost());
		if (LIKELY(sceneRules.IsHost()))
		{
			Assert(playerIdentifier.IsValid());
			Assert(sceneRules.HasPlayerJoined(playerIdentifier), "Can't finish a scene rules that player doesn't participate in!");
			Assert(m_activePlayers.IsSet(playerIdentifier), "Can't finish a scene rules twice!");
			if (LIKELY(sceneRules.HasPlayerJoined(playerIdentifier) && m_activePlayers.Clear(playerIdentifier)))
			{
				OnPlayerFinished(sceneRules, playerIdentifier, finishResult);

				switch (finishResult)
				{
					case GameRulesFinishResult::Success:
						ClientOnPlayerFinishedSuccessInternal(sceneRules, playerIdentifier);
						break;
					case GameRulesFinishResult::Failure:
						ClientOnPlayerFinishedFailureInternal(sceneRules, playerIdentifier);
						break;
				}

				const Optional<Network::Session::BoundComponent*> pBoundComponent =
					sceneRules.FindDataComponentOfType<Network::Session::BoundComponent>();
				Assert(pBoundComponent.IsValid());
				if (LIKELY(pBoundComponent.IsValid()))
				{
					for (const ClientIdentifier::IndexType clientIndex : sceneRules.GetLoadedPlayerIterator())
					{
						const ClientIdentifier clientIdentifier = ClientIdentifier::MakeFromValidIndex(clientIndex);
						switch (finishResult)
						{
							case GameRulesFinishResult::Success:
								pBoundComponent->SendMessageToClient<&FinishModule::ClientOnPlayerFinishedSuccess>(
									sceneRules,
									sceneRules.GetSceneRegistry(),
									clientIdentifier,
									Network::Channel{0},
									playerIdentifier
								);
								break;
							case GameRulesFinishResult::Failure:
								pBoundComponent->SendMessageToClient<&FinishModule::ClientOnPlayerFinishedFailure>(
									sceneRules,
									sceneRules.GetSceneRegistry(),
									clientIdentifier,
									Network::Channel{0},
									playerIdentifier
								);
								break;
						}
					}
				}

				if (Optional<HUDModule*> pHUDModule = sceneRules.FindDataComponentOfType<HUDModule>())
				{
					const GameFramework::PlayerManager& playerManager = System::FindPlugin<GameFramework::Plugin>()->GetPlayerManager();
					if (Optional<const GameFramework::PlayerInfo*> pPlayerInfo = playerManager.FindPlayerInfo(playerIdentifier))
					{
						if (const Optional<Rendering::SceneView*> pSceneView = pPlayerInfo->GetSceneView())
						{
							if (const Optional<SceneViewModeBase*> pPlayViewMode = pSceneView->GetCurrentMode())
							{
								static_cast<GameFramework::PlayViewModeBase&>(*pPlayViewMode).ShowCursor();
							}
						}
					}

					switch (finishResult)
					{
						case GameRulesFinishResult::Success:
						{
							pHUDModule->Notify(PlayerFinishSuccessEvent, playerIdentifier);
						}
						break;
						case GameRulesFinishResult::Failure:
						{
							pHUDModule->Notify(PlayerFinishFailEvent, playerIdentifier);
						}
						break;
					}
				}
			}
		}
	}

	void FinishModule::ClientOnPlayerFinishedSuccess(
		Entity::HierarchyComponentBase& parent,
		Network::Session::BoundComponent&,
		Network::LocalClient&,
		const ClientIdentifier clientIdentifier
	)
	{
		ClientOnPlayerFinishedSuccessInternal(parent.AsExpected<SceneRules>(), clientIdentifier);
	}

	void FinishModule::ClientOnPlayerFinishedSuccessInternal(SceneRules& sceneRules, const ClientIdentifier playerIdentifier)
	{
		if (m_activePlayers.IsSet(playerIdentifier))
		{
			OnPlayerFinished(sceneRules, playerIdentifier, GameRulesFinishResult::Success);
		}
	}

	void FinishModule::ClientOnPlayerFinishedFailure(
		Entity::HierarchyComponentBase& parent,
		Network::Session::BoundComponent&,
		Network::LocalClient&,
		const ClientIdentifier clientIdentifier
	)
	{
		ClientOnPlayerFinishedFailureInternal(parent.AsExpected<SceneRules>(), clientIdentifier);
	}

	void FinishModule::ClientOnPlayerFinishedFailureInternal(SceneRules& sceneRules, const ClientIdentifier playerIdentifier)
	{
		if (m_activePlayers.IsSet(playerIdentifier))
		{
			OnPlayerFinished(sceneRules, playerIdentifier, GameRulesFinishResult::Failure);
		}
	}

	void FinishModule::OnPlayerSpawned(SceneRules&, const ClientIdentifier playerIdentifier, Entity::Component3D&)
	{
		m_activePlayers.Set(playerIdentifier);
	}

	[[maybe_unused]] const bool wasFinishModuleRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<FinishModule>>::Make());
	[[maybe_unused]] const bool wasFinishoduleTypeRegistered = Reflection::Registry::RegisterType<FinishModule>();
}
