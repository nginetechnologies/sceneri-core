#pragma once

#include <GameFramework/Components/SceneRules/Modules/Module.h>

#include <Engine/Entity/ComponentSoftReference.h>
#include <Engine/Entity/Component3D.h>

#include <NetworkingCore/Components/BoundObjectIdentifier.h>

#include <Common/Storage/IdentifierArray.h>
#include <Common/Storage/AtomicIdentifierMask.h>
#include <Common/Storage/Compression/Identifier.h>
#include <Common/Threading/AtomicBool.h>
#include <Common/Function/Event.h>

namespace ngine::Network
{
	struct LocalClient;

	namespace Session
	{
		struct BoundComponent;
	}
}

namespace ngine::GameFramework
{
	struct SpawningModule final : SceneRulesModule
	{
		using BaseType = SceneRulesModule;

		SpawningModule(const SpawningModule& templateComponent, const Cloner& cloner);
		SpawningModule(const Deserializer& deserializer);
		SpawningModule(Initializer&& initializer);

		Event<void(void*, SceneRules&, const ClientIdentifier, Entity::Component3D&), 24> OnPlayerSpawned;

		void OnParentCreated(SceneRules& sceneRules);

		virtual void OnLocalSceneLoaded(SceneRules&, const ClientIdentifier) override;
		virtual void OnGameplayStarted(SceneRules&) override;
		virtual void OnGameplayPaused(SceneRules&) override;
		virtual void OnGameplayResumed(SceneRules&) override;
		virtual void OnGameplayStopped(SceneRules&) override;
		virtual void HostOnPlayerSceneLoaded(SceneRules&, const ClientIdentifier) override;
		virtual void HostOnPlayerLeft(SceneRules&, const ClientIdentifier) override;
		virtual void HostOnPlayerRequestRestart(SceneRules&, const ClientIdentifier) override;
		virtual void OnLocalPlayerPaused(SceneRules&, const ClientIdentifier) override;
		virtual void OnLocalPlayerResumed(SceneRules&, const ClientIdentifier) override;

		void RespawnPlayer(SceneRules&, const ClientIdentifier, const Math::WorldTransform worldTransform);

		using AtomicPlayerMask = Threading::AtomicIdentifierMask<ClientIdentifier>;
		using PlayerIterator = AtomicPlayerMask::SetBitsIterator;

		[[nodiscard]] Optional<Entity::Component3D*> GetPlayerComponent(const ClientIdentifier playerIdentifier) const
		{
			return m_playerInfo[playerIdentifier].pComponent;
		}

		[[nodiscard]] Optional<Entity::Component3D*>
		GetDefaultSpawnPoint(SceneRules& sceneRules, const ClientIdentifier playerIdentifier) const;
		void SetSpawnPoint(
			SceneRules& sceneRules, const ClientIdentifier playerIdentifier, const Optional<Entity::Component3D*> pSpawnPointComponent
		);

		enum class Flags : uint8
		{
			QueriedDefaultSpawnPoint = 1 << 0,
			FoundDefaultSpawnPoint = 1 << 1
		};
	protected:
		friend struct Reflection::ReflectedType<SpawningModule>;

		void QueryDefaultSpawnPoint(SceneRules&);

		void OnPlayerFinishedInternal(SceneRules&, const ClientIdentifier, const GameRulesFinishResult);

		void SpawnPlayer(SceneRules& sceneRules, const ClientIdentifier playerIdentifier);
		bool OnPlayerSpawnedInternal(
			const ClientIdentifier playerIdentifier,
			const Network::BoundObjectIdentifier boundObjectIdentifier,
			const Network::BoundObjectIdentifier controllerBoundObjectIdentifier,
			const Network::BoundObjectIdentifier physicsBoundObjectIdentifier,
			Entity::Component3D& playerComponent,
			const Asset::Guid playerAssetGuid,
			SceneRules& sceneRules
		);
		void OnPlayerSpawnFailedInternal(const ClientIdentifier playerIdentifier);

		struct PlayerSpawnedData
		{
			ClientIdentifier playerIdentifier;
			Network::BoundObjectIdentifier boundObjectIdentifier;
			Network::BoundObjectIdentifier controllerBoundObjectIdentifier;
			Network::BoundObjectIdentifier physicsBoundObjectIdentifier;
			// TODO: Compress
			Math::WorldTransform worldTransform;
			Asset::Guid playerAssetGuid;
		};
		friend struct Reflection::ReflectedType<PlayerSpawnedData>;
		void ClientOnPlayerSpawned(
			Entity::HierarchyComponentBase& parent,
			Network::Session::BoundComponent& boundComponent,
			Network::LocalClient& localClient,
			const PlayerSpawnedData data
		);
		bool SpawnPlayerInternal(
			SceneRules& sceneRules,
			const ClientIdentifier playerIdentifier,
			const Network::BoundObjectIdentifier boundObjectIdentifier,
			const Network::BoundObjectIdentifier controllerBoundObjectIdentifier,
			const Network::BoundObjectIdentifier physicsBoundObjectIdentifier,
			const Optional<Entity::Component3D*> pSpawnPoint
		);
		bool SpawnPlayerInternal(
			SceneRules& sceneRules,
			const ClientIdentifier playerIdentifier,
			const Network::BoundObjectIdentifier boundObjectIdentifier,
			const Network::BoundObjectIdentifier controllerBoundObjectIdentifier,
			const Network::BoundObjectIdentifier physicsBoundObjectIdentifier,
			Math::WorldTransform worldTransform,
			const Asset::Guid playerAssetGuid
		);

		struct PlayerRespawnData
		{
			ClientIdentifier playerIdentifier;
			Math::WorldTransform worldTransform;
		};
		friend struct Reflection::ReflectedType<PlayerRespawnData>;
		void ClientOnPlayerRespawned(
			Entity::HierarchyComponentBase& parent,
			Network::Session::BoundComponent& boundComponent,
			Network::LocalClient& localClient,
			const PlayerRespawnData data
		);
		void
		OnPlayerRespawnedInternal(SceneRules& sceneRules, const ClientIdentifier playerIdentifier, const Math::WorldTransform worldTransform);

		struct PlayerLeftData
		{
			ClientIdentifier playerIdentifier;
		};
		friend struct Reflection::ReflectedType<PlayerLeftData>;
		void ClientOnPlayerLeft(
			Entity::HierarchyComponentBase& parent,
			Network::Session::BoundComponent& boundComponent,
			Network::LocalClient& localClient,
			const PlayerLeftData data
		);
		void OnPlayerLeftInternal(SceneRules& sceneRules, const ClientIdentifier playerIdentifier);
	private:
		Entity::ComponentSoftReference m_defaultSpawnPoint;

		AtomicPlayerMask m_requestedSpawnPlayers;
		AtomicPlayerMask m_spawnedPlayers;

		struct PlayerInfo
		{
			Optional<Entity::Component3D*> pComponent;
			Asset::Guid spawnedAssetGuid;
			Entity::ComponentSoftReference m_spawnPointOverride;
		};
		TIdentifierArray<PlayerInfo, ClientIdentifier> m_playerInfo;

		AtomicEnumFlags<Flags> m_flags;
	};

	ENUM_FLAG_OPERATORS(SpawningModule::Flags);
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<GameFramework::SpawningModule::PlayerSpawnedData>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::SpawningModule::PlayerSpawnedData>(
			"{32C0BA5F-2BF9-46A2-866D-E73CF78759C1}"_guid,
			MAKE_UNICODE_LITERAL("Player Spawned Data"),
			TypeFlags{},
			Tags{},
			Properties{
				Property{
					MAKE_UNICODE_LITERAL("Player Identifier"),
					"playerIdentifier",
					"{6ABC248E-A184-4DCB-8A35-AF7FCE1BA6D0}"_guid,
					MAKE_UNICODE_LITERAL("PlayerSpawnedData"),
					PropertyFlags::SentWithNetworkedFunctions,
					&GameFramework::SpawningModule::PlayerSpawnedData::playerIdentifier
				},
				Property{
					MAKE_UNICODE_LITERAL("Bound Object Identifier"),
					"boundObjectIdentifier",
					"{0B0BD84A-B9B6-41F6-80A2-01CF3A3CA81C}"_guid,
					MAKE_UNICODE_LITERAL("PlayerSpawnedData"),
					PropertyFlags::SentWithNetworkedFunctions,
					&GameFramework::SpawningModule::PlayerSpawnedData::boundObjectIdentifier
				},
				Property{
					MAKE_UNICODE_LITERAL("Controller Bound Object Identifier"),
					"controllerBoundObjectIdentifier",
					"{E2F050BE-C6C5-4241-995F-F7C756D3C830}"_guid,
					MAKE_UNICODE_LITERAL("PlayerSpawnedData"),
					PropertyFlags::SentWithNetworkedFunctions,
					&GameFramework::SpawningModule::PlayerSpawnedData::controllerBoundObjectIdentifier
				},
				Property{
					MAKE_UNICODE_LITERAL("Physics Bound Object Identifier"),
					"physicsBoundObjectIdentifier",
					"{20C3838B-B19B-43FA-8C87-8C5398DF3C82}"_guid,
					MAKE_UNICODE_LITERAL("PlayerSpawnedData"),
					PropertyFlags::SentWithNetworkedFunctions,
					&GameFramework::SpawningModule::PlayerSpawnedData::physicsBoundObjectIdentifier
				},
				Property{
					MAKE_UNICODE_LITERAL("World Transform"),
					"worldTransform",
					"{9AB1D136-7F0A-4307-B7E9-EA67FBD799CE}"_guid,
					MAKE_UNICODE_LITERAL("PlayerSpawnedData"),
					PropertyFlags::SentWithNetworkedFunctions,
					&GameFramework::SpawningModule::PlayerSpawnedData::worldTransform
				},
				Property{
					MAKE_UNICODE_LITERAL("Asset Guid"),
					"playerAssetGuid",
					"{FFD7EF6C-C620-4CFF-8D9E-AA3017AB04A6}"_guid,
					MAKE_UNICODE_LITERAL("PlayerSpawnedData"),
					PropertyFlags::SentWithNetworkedFunctions,
					&GameFramework::SpawningModule::PlayerSpawnedData::playerAssetGuid
				}
			}
		);
	};

	template<>
	struct ReflectedType<GameFramework::SpawningModule::PlayerRespawnData>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::SpawningModule::PlayerSpawnedData>(
			"{EA4EB750-3CDB-43ED-AC32-3783037E47D2}"_guid,
			MAKE_UNICODE_LITERAL("Player Spawned Data"),
			TypeFlags{},
			Tags{},
			Properties{
				Property{
					MAKE_UNICODE_LITERAL("Player Identifier"),
					"playerIdentifier",
					"{FFD7EF6C-C620-4CFF-8D9E-AA3017AB04A6}"_guid,
					MAKE_UNICODE_LITERAL("PlayerSpawnedData"),
					PropertyFlags::SentWithNetworkedFunctions,
					&GameFramework::SpawningModule::PlayerRespawnData::playerIdentifier
				},
				Property{
					MAKE_UNICODE_LITERAL("World Transform"),
					"worldTransform",
					"{747445FA-7BA1-45A3-9702-133C43CF9A7C}"_guid,
					MAKE_UNICODE_LITERAL("PlayerSpawnedData"),
					PropertyFlags::SentWithNetworkedFunctions,
					&GameFramework::SpawningModule::PlayerRespawnData::worldTransform
				}
			}
		);
	};

	template<>
	struct ReflectedType<GameFramework::SpawningModule::PlayerLeftData>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::SpawningModule::PlayerLeftData>(
			"2a7d3e63-d7b0-4e71-9a0b-a8ff352407d6"_guid,
			MAKE_UNICODE_LITERAL("Player Left Data"),
			TypeFlags{},
			Tags{},
			Properties{Property{
				MAKE_UNICODE_LITERAL("Player Identifier"),
				"playerIdentifier",
				"bd653b71-eaf3-480b-ac30-6f014e56e849"_guid,
				MAKE_UNICODE_LITERAL("PlayerSpawnedData"),
				PropertyFlags::SentWithNetworkedFunctions,
				&GameFramework::SpawningModule::PlayerLeftData::playerIdentifier
			}}
		);
	};

	template<>
	struct ReflectedType<GameFramework::SpawningModule>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::SpawningModule>(
			"b60ddb15-deda-47d5-a53b-7a7289bc8ae3"_guid,
			MAKE_UNICODE_LITERAL("Spawnpoint Game Rules Module"),
			TypeFlags{},
			Tags{},
			Properties{},
			Functions{
				Function{
					"{33586EAF-7BF8-4F46-97D3-34FC176C2EB4}"_guid,
					MAKE_UNICODE_LITERAL("Player Spawned"),
					&GameFramework::SpawningModule::ClientOnPlayerSpawned,
					FunctionFlags::HostToClient,
					Reflection::ReturnType{},
					Reflection::Argument{"{F1E5E1DE-99FA-44DB-A0D6-52E5B78D4472}"_guid, MAKE_UNICODE_LITERAL("owner")},
					Reflection::Argument{"ec306a54-3f68-49af-94f3-bffe806b20f3"_guid, MAKE_UNICODE_LITERAL("boundComponent")},
					Reflection::Argument{"bcde10ae-0ba6-4b0c-8dec-1f2f44e4c082"_guid, MAKE_UNICODE_LITERAL("localClient")},
					Reflection::Argument{"9e162937-d204-48ea-a6e6-106d0b5d3cac"_guid, MAKE_UNICODE_LITERAL("data")}
				},
				Function{
					"{D546A991-A8E0-4CE0-988C-BA45B4522E08}"_guid,
					MAKE_UNICODE_LITERAL("Player Respawned"),
					&GameFramework::SpawningModule::ClientOnPlayerRespawned,
					FunctionFlags::HostToClient,
					Reflection::ReturnType{},
					Reflection::Argument{"{F1E5E1DE-99FA-44DB-A0D6-52E5B78D4472}"_guid, MAKE_UNICODE_LITERAL("owner")},
					Reflection::Argument{"ec306a54-3f68-49af-94f3-bffe806b20f3"_guid, MAKE_UNICODE_LITERAL("boundComponent")},
					Reflection::Argument{"bcde10ae-0ba6-4b0c-8dec-1f2f44e4c082"_guid, MAKE_UNICODE_LITERAL("localClient")},
					Reflection::Argument{"9e162937-d204-48ea-a6e6-106d0b5d3cac"_guid, MAKE_UNICODE_LITERAL("data")}
				},
				Function{
					"3cb87fb8-128c-4de3-a608-83432e30f05c"_guid,
					MAKE_UNICODE_LITERAL("Player Left"),
					&GameFramework::SpawningModule::ClientOnPlayerLeft,
					FunctionFlags::HostToClient,
					Reflection::ReturnType{},
					Reflection::Argument{"{F1E5E1DE-99FA-44DB-A0D6-52E5B78D4472}"_guid, MAKE_UNICODE_LITERAL("owner")},
					Reflection::Argument{"ec306a54-3f68-49af-94f3-bffe806b20f3"_guid, MAKE_UNICODE_LITERAL("boundComponent")},
					Reflection::Argument{"bcde10ae-0ba6-4b0c-8dec-1f2f44e4c082"_guid, MAKE_UNICODE_LITERAL("localClient")},
					Reflection::Argument{"9e162937-d204-48ea-a6e6-106d0b5d3cac"_guid, MAKE_UNICODE_LITERAL("data")}
				}
			}
		);
	};
}
