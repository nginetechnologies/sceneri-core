#include "Components/SceneRules/Modules/SpawningModule.h"
#include "Components/SceneRules/Modules/HUDModule.h"
#include "Components/SceneRules/Modules/FinishModule.h"

#include <GameFramework/Components/SceneRules/SceneRules.h>
#include <GameFramework/Components/SpawnPoint.h>
#include <GameFramework/Components/Player/Player.h>
#include <GameFramework/Components/Controllers/WalkingCharacter.h>
#include <GameFramework/Components/Controllers/Vehicle.h>
#include <GameFramework/PlayerInfo.h>
#include <GameFramework/Spawning.h>
#include <GameFramework/Components/Items/FirearmProperties.h>
#include <GameFramework/Plugin.h>

#include <PhysicsCore/Components/Data/BodyComponent.h>
#include <PhysicsCore/Components/CharacterBase.h>
#include <PhysicsCore/Components/Vehicles/Vehicle.h>

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

#include <Engine/Scene/Queries/AsyncOctreeTraversalBoxOverlapJob.h>

#include <Tags.h>
#include <Engine/Tag/TagRegistry.h>

#include <Widgets/Widget.h>

#include <NetworkingCore/Components/ClientComponent.h>
#include <NetworkingCore/Components/LocalClientComponent.h>
#include <NetworkingCore/Components/BoundComponent.h>

#include <Common/Reflection/Registry.inl>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/Threading/Jobs/JobRunnerThread.h>
#include <Common/IO/Log.h>

namespace ngine::GameFramework
{
	SpawningModule::SpawningModule(const SpawningModule& templateComponent, const Cloner& cloner)
		: SceneRulesModule(templateComponent, cloner)
	{
	}

	SpawningModule::SpawningModule(const Deserializer& deserializer)
		: SceneRulesModule(deserializer)
	{
	}

	SpawningModule::SpawningModule(Initializer&& initializer)
		: SceneRulesModule(Forward<Initializer>(initializer))
	{
	}

	void SpawningModule::OnParentCreated(SceneRules& sceneRules)
	{
		if (const Optional<FinishModule*> pFinishModule = sceneRules.FindDataComponentOfType<FinishModule>())
		{
			pFinishModule->OnPlayerFinished.Add(*this, &SpawningModule::OnPlayerFinishedInternal);
		}
	}

	void SpawningModule::QueryDefaultSpawnPoint(SceneRules& sceneRules)
	{
		Assert(sceneRules.IsHost());
		if (sceneRules.IsHost() && m_flags.TrySetFlags(Flags::QueriedDefaultSpawnPoint))
		{
			// Find spawn point
			SceneQueries::AsyncOctreeTraversalBoxOverlapJob* pSpawnPointQueryJob = new SceneQueries::AsyncOctreeTraversalBoxOverlapJob(
				sceneRules.GetSceneRegistry(),
				sceneRules.GetRootSceneComponent().GetRootNode(),
				System::Get<Tag::Registry>().FindOrRegister(Reflection::GetTypeGuid<GameFramework::SpawnPointComponent>()),
				Math::WorldBoundingBox(Math::Radiusf::FromMeters(1000000.f)),
				[this, &sceneRegistry = sceneRules.GetSceneRegistry()](const Optional<Entity::Component3D*> pComponent) -> Memory::CallbackResult
				{
					m_defaultSpawnPoint = Entity::ComponentSoftReference{pComponent, sceneRegistry};
					[[maybe_unused]] const bool wasSet = m_flags.TrySetFlags(Flags::FoundDefaultSpawnPoint);
					Assert(wasSet);
					return Memory::CallbackResult::Break;
				},
				Threading::JobPriority::InteractivityLogic
			);
			pSpawnPointQueryJob->AddSubsequentStage(Threading::CreateCallback(
				[this, &sceneRules](Threading::JobRunnerThread&)
				{
					if (sceneRules.HasGameplayStarted())
					{
						for (const ClientIdentifier::IndexType clientIndex : sceneRules.GetLoadedPlayerIterator())
						{
							const ClientIdentifier clientIdentifier = ClientIdentifier::MakeFromValidIndex(clientIndex);
							if (m_requestedSpawnPlayers.Set(clientIdentifier))
							{
								SpawnPlayer(sceneRules, clientIdentifier);
							}
						}
					}
				},
				Threading::JobPriority::InteractivityLogic
			));
			Threading::JobRunnerThread::GetCurrent()->Queue(*pSpawnPointQueryJob);
		}
	}

	void SpawningModule::OnLocalSceneLoaded(SceneRules& sceneRules, const ClientIdentifier)
	{
		if (sceneRules.IsHost())
		{
			QueryDefaultSpawnPoint(sceneRules);
		}
	}

	void SpawningModule::OnGameplayStarted(SceneRules& sceneRules)
	{
		if (sceneRules.IsHost() && sceneRules.HasGameplayStarted() && m_flags.IsSet(Flags::FoundDefaultSpawnPoint))
		{
			for (const ClientIdentifier::IndexType clientIndex : sceneRules.GetLoadedPlayerIterator())
			{
				const ClientIdentifier clientIdentifier = ClientIdentifier::MakeFromValidIndex(clientIndex);
				if (m_requestedSpawnPlayers.Set(clientIdentifier))
				{
					SpawnPlayer(sceneRules, clientIdentifier);
				}
			}
		}
	}

	void SpawningModule::OnGameplayPaused(SceneRules& sceneRules)
	{
		for (const ClientIdentifier::IndexType clientIndex : sceneRules.GetPlayerIterator())
		{
			const ClientIdentifier clientIdentifier = ClientIdentifier::MakeFromValidIndex(clientIndex);

			if (Optional<Entity::Component3D*> pPlayerComponent = GetPlayerComponent(clientIdentifier))
			{
				pPlayerComponent->Disable();

				if (Optional<Components::WalkingCharacterBase*> pCharacter = pPlayerComponent->FindFirstChildImplementingTypeRecursive<Components::WalkingCharacterBase>())
				{
					pCharacter->Disable();
				}

				if (Optional<Physics::CharacterBase*> pPhysicsCharacter = pPlayerComponent->FindFirstChildImplementingTypeRecursive<Physics::CharacterBase>())
				{
					pPhysicsCharacter->Disable();
				}
			}
		}
	}

	void SpawningModule::OnGameplayResumed(SceneRules& sceneRules)
	{
		for (const ClientIdentifier::IndexType clientIndex : sceneRules.GetPlayerIterator())
		{
			const ClientIdentifier clientIdentifier = ClientIdentifier::MakeFromValidIndex(clientIndex);

			if (Optional<Entity::Component3D*> pPlayerComponent = GetPlayerComponent(clientIdentifier))
			{
				pPlayerComponent->Enable();

				if (Optional<Components::WalkingCharacterBase*> pCharacter = pPlayerComponent->FindFirstChildImplementingTypeRecursive<Components::WalkingCharacterBase>())
				{
					pCharacter->Enable();
				}

				if (Optional<Physics::CharacterBase*> pPhysicsCharacter = pPlayerComponent->FindFirstChildImplementingTypeRecursive<Physics::CharacterBase>())
				{
					pPhysicsCharacter->Enable();
				}
			}
		}
	}

	void SpawningModule::OnLocalPlayerPaused(SceneRules&, const ClientIdentifier clientIdentifier)
	{
		if (Optional<Entity::Component3D*> pPlayerComponent = GetPlayerComponent(clientIdentifier))
		{
			if (Entity::DataComponentResult<Entity::InputComponent> pInputComponent = pPlayerComponent->FindFirstDataComponentOfTypeInChildrenRecursive<Entity::InputComponent>())
			{
				pInputComponent.m_pDataComponentOwner->Disable();
			}
		}
	}

	void SpawningModule::OnLocalPlayerResumed(SceneRules&, const ClientIdentifier clientIdentifier)
	{
		if (Optional<Entity::Component3D*> pPlayerComponent = GetPlayerComponent(clientIdentifier))
		{
			if (Entity::DataComponentResult<Entity::InputComponent> pInputComponent = pPlayerComponent->FindFirstDataComponentOfTypeInChildrenRecursive<Entity::InputComponent>())
			{
				pInputComponent.m_pDataComponentOwner->Enable();
			}
		}
	}

	void SpawningModule::OnGameplayStopped(SceneRules& sceneRules)
	{
		Entity::SceneRegistry& sceneRegistry = sceneRules.GetSceneRegistry();
		for (const ClientIdentifier::IndexType clientIndex : sceneRules.GetPlayerIterator())
		{
			const ClientIdentifier clientIdentifier = ClientIdentifier::MakeFromValidIndex(clientIndex);

			Optional<Entity::Component3D*> pPlayerComponent = m_playerInfo[clientIdentifier].pComponent;
			if (pPlayerComponent.IsValid())
			{
				if (Optional<Entity::InputComponent*> pInputComponent = pPlayerComponent->FindFirstDataComponentOfTypeInChildrenRecursive<Entity::InputComponent>().m_pDataComponent)
				{
					if (Optional<Player*> pPlayerDataComponent = pPlayerComponent->FindDataComponentOfType<Player>())
					{
						pPlayerDataComponent->UnassignInput(*pInputComponent);
					}
				}

				pPlayerComponent->Destroy(sceneRegistry);
				m_playerInfo[clientIdentifier].pComponent = Invalid;
				m_playerInfo[clientIdentifier].m_spawnPointOverride = {};
			}
		}
	}

	inline static constexpr Asset::Guid DefaultPlayerAssetGuid = "1ca23fef-ddd8-4790-bf50-da8e3efc7a18"_asset;

	Optional<Entity::Component3D*> SpawningModule::GetDefaultSpawnPoint(SceneRules& sceneRules, const ClientIdentifier playerIdentifier) const
	{
		if (m_playerInfo[playerIdentifier].m_spawnPointOverride.IsPotentiallyValid())
		{
			if (const Optional<Entity::Component3D*> pSpawnPoint = m_playerInfo[playerIdentifier].m_spawnPointOverride.Find<Entity::Component3D>(sceneRules.GetSceneRegistry()))
			{
				return pSpawnPoint;
			}
		}

		return m_defaultSpawnPoint.Find<Entity::Component3D>(sceneRules.GetSceneRegistry());
	}

	void SpawningModule::SetSpawnPoint(
		SceneRules& sceneRules, const ClientIdentifier playerIdentifier, const Optional<Entity::Component3D*> pSpawnPointComponent
	)
	{
		m_playerInfo[playerIdentifier].m_spawnPointOverride =
			Entity::ComponentSoftReference{pSpawnPointComponent, sceneRules.GetSceneRegistry()};
	}

	void SpawningModule::SpawnPlayer(SceneRules& sceneRules, const ClientIdentifier playerIdentifier)
	{
		Assert(sceneRules.IsHost());

		const Optional<Entity::Component3D*> pSpawnPoint = GetDefaultSpawnPoint(sceneRules, playerIdentifier);
		Assert(pSpawnPoint.IsValid());
		if (LIKELY(pSpawnPoint.IsValid()))
		{
			SpawnPlayerInternal(
				sceneRules,
				playerIdentifier,
				Network::BoundObjectIdentifier{},
				Network::BoundObjectIdentifier{},
				Network::BoundObjectIdentifier{},
				pSpawnPoint
			);
		}
	}

	void SpawningModule::ClientOnPlayerSpawned(
		Entity::HierarchyComponentBase& parent, Network::Session::BoundComponent&, Network::LocalClient&, const PlayerSpawnedData data
	)
	{
		if (m_requestedSpawnPlayers.Set(data.playerIdentifier))
		{
			m_playerInfo[data.playerIdentifier].spawnedAssetGuid = data.playerAssetGuid;
			SpawnPlayerInternal(
				parent.AsExpected<SceneRules>(),
				data.playerIdentifier,
				data.boundObjectIdentifier,
				data.controllerBoundObjectIdentifier,
				data.physicsBoundObjectIdentifier,
				data.worldTransform,
				data.playerAssetGuid
			);
		}
	}

	bool SpawningModule::SpawnPlayerInternal(
		SceneRules& sceneRules,
		const ClientIdentifier playerIdentifier,
		const Network::BoundObjectIdentifier boundObjectIdentifier,
		const Network::BoundObjectIdentifier controllerBoundObjectIdentifier,
		const Network::BoundObjectIdentifier physicsBoundObjectIdentifier,
		const Optional<Entity::Component3D*> pSpawnPoint
	)
	{
		if (pSpawnPoint.IsValid())
		{
			GameFramework::SpawnPointComponent& spawnPointComponent = pSpawnPoint->AsExpected<GameFramework::SpawnPointComponent>();

			const Asset::Guid spawnedAssetGuid = spawnPointComponent.GetSpawnedAssetGuid().IsValid() ? spawnPointComponent.GetSpawnedAssetGuid()
			                                                                                         : DefaultPlayerAssetGuid;
			m_playerInfo[playerIdentifier].spawnedAssetGuid = spawnedAssetGuid;

			return SpawnPlayerInternal(
				sceneRules,
				playerIdentifier,
				boundObjectIdentifier,
				controllerBoundObjectIdentifier,
				physicsBoundObjectIdentifier,
				spawnPointComponent.GetWorldTransform(),
				spawnedAssetGuid
			);
		}
		else
		{
			LogError("Failed to find spawn point!");
			Assert(false, "Failed to find spawn point!");
			OnPlayerSpawnFailedInternal(playerIdentifier);
			return false;
		}
	}

	bool SpawningModule::SpawnPlayerInternal(
		SceneRules& sceneRules,
		const ClientIdentifier playerIdentifier,
		const Network::BoundObjectIdentifier boundObjectIdentifier,
		const Network::BoundObjectIdentifier controllerBoundObjectIdentifier,
		const Network::BoundObjectIdentifier physicsBoundObjectIdentifier,
		Math::WorldTransform worldTransform,
		const Asset::Guid playerAssetGuid
	)
	{
		SpawnInitializer initializer{sceneRules.GetRootScene(), sceneRules.GetParentSceneComponent(), worldTransform};
		return GameFramework::SpawnAsset(
			playerAssetGuid,
			Move(initializer),
			[this,
		   &sceneRules,
		   playerIdentifier,
		   boundObjectIdentifier,
		   controllerBoundObjectIdentifier,
		   physicsBoundObjectIdentifier,
		   playerAssetGuid](const Optional<Entity::Component3D*> pSpawnedAsset)
			{
				if (LIKELY(pSpawnedAsset.IsValid()))
				{
					OnPlayerSpawnedInternal(
						playerIdentifier,
						boundObjectIdentifier,
						controllerBoundObjectIdentifier,
						physicsBoundObjectIdentifier,
						*pSpawnedAsset,
						playerAssetGuid,
						sceneRules
					);
				}
				else
				{
					LogWarning("Failed to spawn player!");
					Assert(false, "Failed to spawn player!");
					OnPlayerSpawnFailedInternal(playerIdentifier);
				}

				[[maybe_unused]] const bool wasSet = m_spawnedPlayers.Set(playerIdentifier);
				Assert(wasSet);
			}
		);
	}

	void SpawningModule::HostOnPlayerSceneLoaded(SceneRules& sceneRules, const ClientIdentifier playerIdentifier)
	{
		QueryDefaultSpawnPoint(sceneRules);

		const Optional<Network::Session::BoundComponent*> pBoundComponent =
			sceneRules.FindDataComponentOfType<Network::Session::BoundComponent>();
		Assert(pBoundComponent.IsValid());
		if (LIKELY(pBoundComponent.IsValid()))
		{
			for (const ClientIdentifier::IndexType clientIndex : m_spawnedPlayers.GetSetBitsIterator())
			{
				const ClientIdentifier clientIdentifier = ClientIdentifier::MakeFromValidIndex(clientIndex);
				Optional<Entity::Component3D*> pPlayerComponent = m_playerInfo[clientIdentifier].pComponent;
				Assert(pPlayerComponent.IsValid());
				if (LIKELY(pPlayerComponent.IsValid()))
				{
					Optional<Network::Session::BoundComponent*> pPlayerBoundComponent =
						pPlayerComponent->FindDataComponentOfType<Network::Session::BoundComponent>();
					Assert(pPlayerBoundComponent.IsValid());
					if (LIKELY(pPlayerBoundComponent.IsValid()))
					{
						// Bind walking character or vehicle if present
						Optional<Network::Session::BoundComponent*> pControllerBoundComponent;
						if (const Optional<Components::WalkingCharacterBase*> pWalkingCharacter = pPlayerComponent->FindFirstChildImplementingTypeRecursive<Components::WalkingCharacterBase>())
						{
							pControllerBoundComponent = pWalkingCharacter->FindDataComponentOfType<Network::Session::BoundComponent>();
						}
						else if (const Optional<VehicleController*> pVehicleController = pPlayerComponent->FindFirstChildOfTypeRecursive<VehicleController>())
						{
							pControllerBoundComponent = pVehicleController->FindDataComponentOfType<Network::Session::BoundComponent>();
						}

						Optional<Network::Session::BoundComponent*> pPhysicsBoundComponent;
						if (Optional<Physics::CharacterBase*> pPhysicsCharacter = pPlayerComponent->FindFirstChildImplementingTypeRecursive<Physics::CharacterBase>())
						{
							pPhysicsBoundComponent = pPhysicsCharacter->FindDataComponentOfType<Network::Session::BoundComponent>();
						}
						else if (Optional<Physics::Vehicle*> pPhysicsVehicle = pPlayerComponent->FindFirstChildOfTypeRecursive<Physics::Vehicle>())
						{
							pPhysicsBoundComponent = pPhysicsVehicle->FindDataComponentOfType<Network::Session::BoundComponent>();
						}

						pBoundComponent->SendMessageToClient<&SpawningModule::ClientOnPlayerSpawned>(
							sceneRules,
							sceneRules.GetSceneRegistry(),
							playerIdentifier,
							Network::Channel{0},
							PlayerSpawnedData{
								clientIdentifier,
								pPlayerBoundComponent->GetIdentifier(),
								pControllerBoundComponent.IsValid() ? pControllerBoundComponent->GetIdentifier() : Network::BoundObjectIdentifier{},
								pPhysicsBoundComponent.IsValid() ? pPhysicsBoundComponent->GetIdentifier() : Network::BoundObjectIdentifier{},
								pPlayerComponent->GetWorldTransform(),
								m_playerInfo[clientIdentifier].spawnedAssetGuid
							}
						);
					}
				}
			}
		}

		if (sceneRules.HasGameplayStarted() && m_flags.IsSet(Flags::FoundDefaultSpawnPoint))
		{
			if (m_requestedSpawnPlayers.Set(playerIdentifier))
			{
				SpawnPlayer(sceneRules, playerIdentifier);
			}
		}
	}

	bool SpawningModule::OnPlayerSpawnedInternal(
		const ClientIdentifier playerIdentifier,
		Network::BoundObjectIdentifier playerBoundObjectIdentifier,
		Network::BoundObjectIdentifier controllerBoundObjectIdentifier,
		Network::BoundObjectIdentifier physicsBoundObjectIdentifier,
		Entity::Component3D& playerComponent,
		const Asset::Guid playerAssetGuid,
		SceneRules& sceneRules
	)
	{
		Entity::SceneRegistry& sceneRegistry = playerComponent.GetSceneRegistry();

		Assert(playerIdentifier.IsValid());
		m_playerInfo[playerIdentifier].pComponent = &playerComponent;
		Optional<Player*> pPlayerComponent = playerComponent.CreateDataComponent<Player>(
			Player::Initializer(Entity::Data::Component::DynamicInitializer{playerComponent, sceneRegistry}, playerIdentifier)
		);
		Assert(pPlayerComponent.IsValid());
		if (LIKELY(pPlayerComponent.IsValid()))
		{
			if (const Optional<Network::Session::Client*> pClient = Network::Session::Client::Find(playerIdentifier, playerComponent.GetRootParent()))
			{
				if (pClient->HasDataComponentOfType<Network::Session::LocalClient>(sceneRegistry))
				{
					Optional<Entity::Data::Tags*> pPlayerTagsComponent = playerComponent.FindDataComponentOfType<Entity::Data::Tags>();
					if (pPlayerTagsComponent.IsInvalid())
					{
						pPlayerTagsComponent =
							playerComponent.CreateDataComponent<Entity::Data::Tags>(Entity::Data::Tags::Initializer{playerComponent, sceneRegistry});
					}

					Tag::Registry& tagRegistry = System::Get<Tag::Registry>();
					pPlayerTagsComponent
						->SetTag(playerComponent.GetIdentifier(), sceneRegistry, tagRegistry.FindOrRegister(Tags::LocalPlayerTagGuid));

					if (Optional<Entity::CameraComponent*> pCamera = playerComponent.FindFirstChildOfTypeRecursive<Entity::CameraComponent>())
					{
						pPlayerComponent->ChangeCamera(*pCamera);
					}

					if (Optional<Entity::InputComponent*> pInputComponent = playerComponent.FindFirstDataComponentOfTypeInChildrenRecursive<Entity::InputComponent>().m_pDataComponent)
					{
						pPlayerComponent->AssignInput(*pInputComponent);
					}
				}
			}

			OnPlayerSpawned(sceneRules, playerIdentifier, playerComponent);
		}

		const GameFramework::PlayerManager& playerManager = System::FindPlugin<GameFramework::Plugin>()->GetPlayerManager();
		if (Optional<const GameFramework::PlayerInfo*> pPlayerInfo = playerManager.FindPlayerInfo(playerIdentifier))
		{
			if (const Optional<Widgets::Widget*> pWidget = pPlayerInfo->GetHUD())
			{
				Context::EventManager eventManager(pWidget->GetRootWidget(), sceneRegistry);
				// Hide the loading screen
				eventManager.Notify("ea5387e5-a4ca-4ab8-afff-061fab27ca5d"_guid);
			}
		}

		// Bind walking character or vehicle if present
		Optional<Entity::Component3D*> pControllerComponent;
		if (const Optional<Components::WalkingCharacterBase*> pWalkingCharacter = playerComponent.FindFirstChildImplementingTypeRecursive<Components::WalkingCharacterBase>())
		{
			pControllerComponent = pWalkingCharacter;
		}
		else if (const Optional<VehicleController*> pVehicleController = playerComponent.FindFirstChildOfTypeRecursive<VehicleController>())
		{
			pControllerComponent = pVehicleController;
		}
		if (pControllerComponent.IsValid())
		{
			Optional<Network::Session::BoundComponent*> pControllerBoundComponent =
				pControllerComponent->FindDataComponentOfType<Network::Session::BoundComponent>();
			if (sceneRules.IsHost())
			{
				if (pControllerBoundComponent.IsInvalid())
				{
					pControllerBoundComponent = pControllerComponent->CreateDataComponent<Network::Session::BoundComponent>(
						Network::Session::BoundComponent::Initializer{*pControllerComponent, sceneRegistry}
					);
				}

				[[maybe_unused]] const bool wasAuthorityDelegated =
					pControllerBoundComponent->DelegateAuthority(*pControllerComponent, sceneRegistry, playerIdentifier);
				Assert(wasAuthorityDelegated);
				controllerBoundObjectIdentifier = pControllerBoundComponent->GetIdentifier();
			}
			else
			{
				Assert(controllerBoundObjectIdentifier.IsValid());
				if (pControllerBoundComponent.IsValid())
				{
					Assert(pControllerBoundComponent->GetIdentifier() == controllerBoundObjectIdentifier);
				}
				else
				{
					pControllerBoundComponent = pControllerComponent->CreateDataComponent<Network::Session::BoundComponent>(
						*pControllerComponent,
						sceneRegistry,
						controllerBoundObjectIdentifier
					);
				}
			}
		}

		Optional<Entity::Component3D*> pPhysicsComponent;
		if (Optional<Physics::CharacterBase*> pPhysicsCharacter = playerComponent.FindFirstChildImplementingTypeRecursive<Physics::CharacterBase>())
		{
			pPhysicsComponent = pPhysicsCharacter;
		}
		else if (Optional<Physics::Vehicle*> pPhysicsVehicle = playerComponent.FindFirstChildOfTypeRecursive<Physics::Vehicle>())
		{
			pPhysicsComponent = pPhysicsVehicle;
		}
		if (pPhysicsComponent.IsValid())
		{
			Optional<Network::Session::BoundComponent*> pPhysicsBoundComponent =
				pPhysicsComponent->FindDataComponentOfType<Network::Session::BoundComponent>();
			if (sceneRules.IsHost())
			{
				if (pPhysicsBoundComponent.IsInvalid())
				{
					pPhysicsBoundComponent = pPhysicsComponent->CreateDataComponent<Network::Session::BoundComponent>(
						Network::Session::BoundComponent::Initializer{*pPhysicsComponent, sceneRegistry}
					);
				}

				physicsBoundObjectIdentifier = pPhysicsBoundComponent->GetIdentifier();
			}
			else
			{
				Assert(physicsBoundObjectIdentifier.IsValid());
				if (pPhysicsBoundComponent.IsValid())
				{
					Assert(pPhysicsBoundComponent->GetIdentifier() == physicsBoundObjectIdentifier);
				}
				else
				{
					pPhysicsBoundComponent = pPhysicsComponent->CreateDataComponent<Network::Session::BoundComponent>(
						*pPhysicsComponent,
						sceneRegistry,
						physicsBoundObjectIdentifier
					);
				}
			}
		}

		if (sceneRules.IsHost())
		{
			Assert(playerBoundObjectIdentifier.IsInvalid());
			Optional<Network::Session::BoundComponent*> pPlayerBoundComponent =
				playerComponent.FindDataComponentOfType<Network::Session::BoundComponent>();
			if (pPlayerBoundComponent.IsInvalid())
			{
				pPlayerBoundComponent = playerComponent.CreateDataComponent<Network::Session::BoundComponent>(
					Network::Session::BoundComponent::Initializer{playerComponent, sceneRegistry}
				);
			}

			[[maybe_unused]] const bool wasAuthorityDelegated =
				pPlayerBoundComponent->DelegateAuthority(playerComponent, sceneRegistry, playerIdentifier);
			Assert(wasAuthorityDelegated);

			const Optional<Network::Session::BoundComponent*> pBoundComponent =
				sceneRules.FindDataComponentOfType<Network::Session::BoundComponent>();
			Assert(pBoundComponent.IsValid());
			if (LIKELY(pBoundComponent.IsValid()))
			{
				playerBoundObjectIdentifier = pPlayerBoundComponent->GetIdentifier();

				for (const ClientIdentifier::IndexType clientIndex : sceneRules.GetLoadedPlayerIterator())
				{
					const ClientIdentifier clientIdentifier = ClientIdentifier::MakeFromValidIndex(clientIndex);
					pBoundComponent->SendMessageToClient<&SpawningModule::ClientOnPlayerSpawned>(
						sceneRules,
						sceneRegistry,
						clientIdentifier,
						Network::Channel{0},
						PlayerSpawnedData{
							playerIdentifier,
							playerBoundObjectIdentifier,
							controllerBoundObjectIdentifier,
							physicsBoundObjectIdentifier,
							playerComponent.GetWorldTransform(),
							playerAssetGuid
						}
					);
				}
			}
		}
		else
		{
			Assert(playerBoundObjectIdentifier.IsValid());
			Optional<Network::Session::BoundComponent*> pPlayerBoundComponent =
				playerComponent.FindDataComponentOfType<Network::Session::BoundComponent>();
			if (pPlayerBoundComponent.IsValid())
			{
				Assert(pPlayerBoundComponent->GetIdentifier() == playerBoundObjectIdentifier);
			}
			else
			{
				pPlayerBoundComponent = playerComponent.CreateDataComponent<Network::Session::BoundComponent>(
					playerComponent,
					sceneRegistry,
					playerBoundObjectIdentifier
				);
			}
		}

		return pPlayerComponent.IsValid();
	}

	void SpawningModule::OnPlayerSpawnFailedInternal(const ClientIdentifier playerIdentifier)
	{
		const GameFramework::PlayerManager& playerManager = System::FindPlugin<GameFramework::Plugin>()->GetPlayerManager();
		if (Optional<const GameFramework::PlayerInfo*> pPlayerInfo = playerManager.FindPlayerInfo(playerIdentifier))
		{
			if (const Optional<Widgets::Widget*> pWidget = pPlayerInfo->GetHUD())
			{
				Context::EventManager eventManager(pWidget->GetRootWidget(), pWidget->GetSceneRegistry());
				// Hide the loading screen
				eventManager.Notify("ea5387e5-a4ca-4ab8-afff-061fab27ca5d"_guid);
			}
		}
	}

	void SpawningModule::HostOnPlayerLeft(SceneRules& sceneRules, const ClientIdentifier playerIdentifier)
	{
		OnPlayerLeftInternal(sceneRules, playerIdentifier);

		const Optional<Network::Session::BoundComponent*> pBoundComponent =
			sceneRules.FindDataComponentOfType<Network::Session::BoundComponent>();
		Assert(pBoundComponent.IsValid());
		if (LIKELY(pBoundComponent.IsValid()))
		{
			for (const ClientIdentifier::IndexType clientIndex : sceneRules.GetLoadedPlayerIterator())
			{
				const ClientIdentifier clientIdentifier = ClientIdentifier::MakeFromValidIndex(clientIndex);
				pBoundComponent->SendMessageToClient<&SpawningModule::ClientOnPlayerLeft>(
					sceneRules,
					sceneRules.GetSceneRegistry(),
					clientIdentifier,
					Network::Channel{0},
					PlayerLeftData{playerIdentifier}
				);
			}
		}
	}

	void SpawningModule::HostOnPlayerRequestRestart(SceneRules& sceneRules, const ClientIdentifier playerIdentifier)
	{
		Entity::SceneRegistry& sceneRegistry = sceneRules.GetSceneRegistry();
		Optional<Entity::Component3D*> pPlayerComponent = m_playerInfo[playerIdentifier].pComponent;
		if (pPlayerComponent.IsValid())
		{
			if (Optional<Entity::InputComponent*> pInputComponent = pPlayerComponent->FindFirstDataComponentOfTypeInChildrenRecursive<Entity::InputComponent>().m_pDataComponent)
			{
				if (Optional<Player*> pPlayerDataComponent = pPlayerComponent->FindDataComponentOfType<Player>())
				{
					pPlayerDataComponent->UnassignInput(*pInputComponent);
				}
			}

			pPlayerComponent->Destroy(sceneRegistry);
			m_playerInfo[playerIdentifier].pComponent = Invalid;
			m_playerInfo[playerIdentifier].m_spawnPointOverride = {};
		}

		m_spawnedPlayers.Clear(playerIdentifier);
		m_requestedSpawnPlayers.Clear(playerIdentifier);

		if (sceneRules.HasGameplayStarted() && m_flags.IsSet(Flags::FoundDefaultSpawnPoint))
		{
			if (m_requestedSpawnPlayers.Set(playerIdentifier))
			{
				SpawnPlayer(sceneRules, playerIdentifier);
			}
		}
	}

	void SpawningModule::ClientOnPlayerLeft(
		Entity::HierarchyComponentBase& parent, Network::Session::BoundComponent&, Network::LocalClient&, const PlayerLeftData data
	)
	{
		OnPlayerLeftInternal(parent.AsExpected<SceneRules>(), data.playerIdentifier);
	}

	void SpawningModule::OnPlayerLeftInternal(SceneRules& sceneRules, const ClientIdentifier playerIdentifier)
	{
		Entity::SceneRegistry& sceneRegistry = sceneRules.GetSceneRegistry();
		Optional<Entity::Component3D*> pPlayerComponent = m_playerInfo[playerIdentifier].pComponent;
		if (pPlayerComponent.IsValid())
		{
			if (Optional<Entity::InputComponent*> pInputComponent = pPlayerComponent->FindFirstDataComponentOfTypeInChildrenRecursive<Entity::InputComponent>().m_pDataComponent)
			{
				if (Optional<Player*> pPlayerDataComponent = pPlayerComponent->FindDataComponentOfType<Player>())
				{
					pPlayerDataComponent->UnassignInput(*pInputComponent);
				}
			}

			pPlayerComponent->Destroy(sceneRegistry);
			m_playerInfo[playerIdentifier].pComponent = Invalid;
		}

		m_spawnedPlayers.Clear(playerIdentifier);
		m_requestedSpawnPlayers.Clear(playerIdentifier);
	}

	void
	SpawningModule::OnPlayerFinishedInternal(SceneRules& sceneRules, const ClientIdentifier playerIdentifier, const GameRulesFinishResult)
	{
		if (Optional<Entity::Component3D*> pPlayerComponent = GetPlayerComponent(playerIdentifier))
		{
			pPlayerComponent->Disable();

			if (Optional<Components::WalkingCharacterBase*> pCharacter = pPlayerComponent->FindFirstChildImplementingTypeRecursive<Components::WalkingCharacterBase>(sceneRules.GetSceneRegistry()))
			{
				pCharacter->Disable();
			}

			if (Optional<Physics::CharacterBase*> pPhysicsCharacter = pPlayerComponent->FindFirstChildImplementingTypeRecursive<Physics::CharacterBase>())
			{
				pPhysicsCharacter->Disable();
			}

			if (const Entity::DataComponentResult<Entity::CameraController> pCameraController = pPlayerComponent->FindFirstDataComponentOfTypeInSelfAndChildrenRecursive<Entity::CameraController>(sceneRules.GetSceneRegistry()))
			{
				pCameraController.m_pDataComponentOwner->Disable();
			}
		}
	}

	void
	SpawningModule::RespawnPlayer(SceneRules& sceneRules, const ClientIdentifier playerIdentifier, const Math::WorldTransform worldTransform)
	{
		Assert(sceneRules.IsHost());
		if (LIKELY(sceneRules.IsHost()))
		{
			OnPlayerRespawnedInternal(sceneRules, playerIdentifier, worldTransform);

			const Optional<Network::Session::BoundComponent*> pBoundComponent =
				sceneRules.FindDataComponentOfType<Network::Session::BoundComponent>();
			Assert(pBoundComponent.IsValid());
			if (LIKELY(pBoundComponent.IsValid()))
			{
				for (const ClientIdentifier::IndexType clientIndex : sceneRules.GetLoadedPlayerIterator())
				{
					const ClientIdentifier clientIdentifier = ClientIdentifier::MakeFromValidIndex(clientIndex);
					pBoundComponent->SendMessageToClient<&SpawningModule::ClientOnPlayerRespawned>(
						sceneRules,
						sceneRules.GetSceneRegistry(),
						clientIdentifier,
						Network::Channel{0},
						PlayerRespawnData{playerIdentifier, worldTransform}
					);
				}
			}
		}
	}

	void SpawningModule::ClientOnPlayerRespawned(
		Entity::HierarchyComponentBase& parent, Network::Session::BoundComponent&, Network::LocalClient&, const PlayerRespawnData data
	)
	{
		OnPlayerRespawnedInternal(parent.AsExpected<SceneRules>(), data.playerIdentifier, data.worldTransform);
	}

	void SpawningModule::OnPlayerRespawnedInternal(
		SceneRules& sceneRules, const ClientIdentifier playerIdentifier, const Math::WorldTransform worldTransform
	)
	{
		Optional<Entity::Component3D*> pPlayerComponent = GetPlayerComponent(playerIdentifier);
		Assert(pPlayerComponent.IsValid());
		if (pPlayerComponent.IsValid())
		{
			if (Entity::Component3D::DataComponentResult<Physics::Data::Body> bodyComponent = pPlayerComponent->FindFirstDataComponentOfTypeInChildrenRecursive<Physics::Data::Body>())
			{
				if (!bodyComponent.m_pDataComponentOwner->GetWorldTransform().IsEquivalentTo(worldTransform))
				{
					bodyComponent.m_pDataComponentOwner->SetWorldTransform(worldTransform);
				}
			}

			if (!pPlayerComponent->GetWorldTransform().IsEquivalentTo(worldTransform))
			{
				pPlayerComponent->SetWorldTransform(worldTransform);
			}

			OnPlayerSpawned(sceneRules, playerIdentifier, *pPlayerComponent);
		}
	}

	[[maybe_unused]] const bool wasSpawningModuleRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<SpawningModule>>::Make());
	[[maybe_unused]] const bool wasSpawningModuleTypeRegistered = Reflection::Registry::RegisterType<SpawningModule>();
}
