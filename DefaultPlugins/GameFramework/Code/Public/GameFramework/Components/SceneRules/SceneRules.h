#pragma once

#include <Engine/Entity/Data/Component3D.h>
#include <Engine/Entity/Component3D.h>

#include <Common/AtomicEnumFlags.h>
#include <Common/Storage/IdentifierArray.h>
#include <Common/Storage/IdentifierMask.h>
#include <Common/Storage/AtomicIdentifierMask.h>

#include <Common/Compression/Compressor.h>
#include <Common/Storage/Compression/Identifier.h>

#include <NetworkingCore/Client/ClientIdentifier.h>

namespace ngine::Entity
{
	struct RootSceneComponent;
}

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
	struct PlayViewModeBase;
	using ClientIdentifier = Network::ClientIdentifier;

	struct SceneRules : Entity::Component3D
	{
		using InstanceIdentifier = TIdentifier<uint32, 3>;
		using BaseType = Entity::Component3D;

		using PlayerMask = IdentifierMask<ClientIdentifier>;
		using AtomicPlayerMask = Threading::AtomicIdentifierMask<ClientIdentifier>;

		enum class Flags : uint8
		{
			//! Whether gameplay has started and players can participate
			HasGameplayStarted = 1 << 0,
			//! Whether the game rules has been paused for all players
			IsGameplayPaused = 1 << 1,
			IsHost = 1 << 2
		};

		SceneRules(const SceneRules& templateComponent, const Cloner& cloner);
		SceneRules(const Deserializer& deserializer);
		SceneRules(Initializer&& initializer);
		virtual ~SceneRules() = default;

		[[nodiscard]] static Optional<SceneRules*> Find(Entity::HierarchyComponentBase& rootSceneComponent);
		template<typename ModuleDataComponentType>
		[[nodiscard]] static Entity::DataComponentResult<ModuleDataComponentType> FindModule(Entity::HierarchyComponentBase& rootSceneComponent)
		{
			if (const Optional<SceneRules*> pSceneRules = Find(rootSceneComponent))
			{
				return {pSceneRules->FindDataComponentOfType<ModuleDataComponentType>(), pSceneRules};
			}
			else
			{
				return {};
			}
		}

		[[nodiscard]] bool IsHost() const
		{
			return m_flags.IsSet(Flags::IsHost);
		}
		[[nodiscard]] bool HasPlayerJoined(const ClientIdentifier playerIdentifier) const
		{
			return m_players.IsSet(playerIdentifier);
		}

		void OnLocalPlayerJoined(const ClientIdentifier playerIdentifier);
		void OnLocalPlayerLoadedScene(const ClientIdentifier playerIdentifier);
		void OnLocalPlayerLeft(const ClientIdentifier playerIdentifier);
		void OnLocalPlayerRequestRestart(const ClientIdentifier playerIdentifier);
		void OnLocalPlayerRequestPause(const ClientIdentifier playerIdentifier);
		void OnLocalPlayerRequestResume(const ClientIdentifier playerIdentifier);

		bool Start();
		bool Pause();
		bool Resume();
		bool Restart();
		bool Stop();

		[[nodiscard]] bool HasGameplayStarted() const
		{
			return m_flags.IsSet(Flags::HasGameplayStarted);
		}
		[[nodiscard]] bool IsGameplayPaused() const
		{
			return m_flags.IsSet(Flags::IsGameplayPaused);
		}

		[[nodiscard]] const AtomicPlayerMask& GetPlayersMask() const
		{
			return m_players;
		}
		using PlayerIterator = AtomicPlayerMask::SetBitsIterator;
		//! Gets an iterator to all participating players
		[[nodiscard]] PlayerIterator GetPlayerIterator() const
		{
			return m_players.GetSetBitsIterator();
		}
		//! Gets the number of participating players
		[[nodiscard]] ClientIdentifier::IndexType GetPlayerCount() const
		{
			return m_players.GetNumberOfSetBits();
		}
		//! Gets an iterator to all local players
		[[nodiscard]] AtomicPlayerMask::SetBitsIterator GetLocalPlayerIterator() const
		{
			return m_localPlayers.GetSetBitsIterator();
		}
		//! Gets an iterator to all players that have finished loading
		[[nodiscard]] AtomicPlayerMask::SetBitsIterator GetLoadedPlayerIterator() const
		{
			return m_loadedPlayers.GetSetBitsIterator();
		}
	protected:
		friend struct Reflection::ReflectedType<SceneRules>;

		struct GameplayStartedData
		{
		};
		void OnClientGameplayStarted(
			Network::Session::BoundComponent& boundComponent, Network::LocalClient& localClient, const GameplayStartedData data
		);
		void OnGameplayStartedInternal();

		struct GameplayPausedData
		{
		};
		void OnClientGameplayPaused(
			Network::Session::BoundComponent& boundComponent, Network::LocalClient& localClient, const GameplayPausedData data
		);
		void OnGameplayPausedInternal();

		struct GameplayResumedData
		{
		};
		void OnClientGameplayResumed(
			Network::Session::BoundComponent& boundComponent, Network::LocalClient& localClient, const GameplayResumedData data
		);
		void OnGameplayResumedInternal();

		struct GameplayStoppedData
		{
		};
		void OnClientGameplayStopped(
			Network::Session::BoundComponent& boundComponent, Network::LocalClient& localClient, const GameplayStoppedData data
		);
		void OnGameplayStoppedInternal();

		void HostOnPlayerJoined(
			Network::Session::BoundComponent& boundComponent, Network::LocalClient& localClient, const ClientIdentifier clientIdentifier
		);
		void HostOnPlayerJoinedInternal(const ClientIdentifier playerIdentifier);

		void HostOnPlayerLoadedScene(
			Network::Session::BoundComponent& boundComponent, Network::LocalClient& localClient, const ClientIdentifier clientIdentifier
		);
		void HostOnPlayerLoadedSceneInternal(const ClientIdentifier playerIdentifier);

		void HostOnPlayerLeft(
			Network::Session::BoundComponent& boundComponent, Network::LocalClient& localClient, const ClientIdentifier clientIdentifier
		);
		void HostOnPlayerLeftInternal(const ClientIdentifier playerIdentifier);

		void HostOnPlayerRequestRestart(
			Network::Session::BoundComponent& boundComponent, Network::LocalClient& localClient, const ClientIdentifier clientIdentifier
		);
		void HostOnPlayerRequestRestartInternal(const ClientIdentifier playerIdentifier);

		void HostOnPlayerRequestPause(
			Network::Session::BoundComponent& boundComponent, Network::LocalClient& localClient, const ClientIdentifier clientIdentifier
		);
		void HostOnPlayerRequestPauseInternal(const ClientIdentifier playerIdentifier);

		void HostOnPlayerRequestResume(
			Network::Session::BoundComponent& boundComponent, Network::LocalClient& localClient, const ClientIdentifier clientIdentifier
		);
		void HostOnPlayerRequestResumeInternal(const ClientIdentifier playerIdentifier);
	protected:
		AtomicEnumFlags<Flags> m_flags;
		AtomicPlayerMask m_players;
		AtomicPlayerMask m_localPlayers;
		//! Players that have finished loading the scene
		AtomicPlayerMask m_loadedPlayers;
	};

	ENUM_FLAG_OPERATORS(SceneRules::Flags);
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<GameFramework::SceneRules>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::SceneRules>(
			"e31a9bd0-5080-4319-b0ea-290782393093"_guid,
			MAKE_UNICODE_LITERAL("Scene Rules"),
			TypeFlags{},
			Tags{},
			Properties{},
			Functions{
				Function{
					"{B1DE55B5-83BA-4118-B97D-CF89B8CD6161}"_guid,
					MAKE_UNICODE_LITERAL("Start Gameplay"),
					&GameFramework::SceneRules::OnClientGameplayStarted,
					FunctionFlags::HostToClient,
					Reflection::ReturnType{},
					Reflection::Argument{"ec306a54-3f68-49af-94f3-bffe806b20f3"_guid, MAKE_UNICODE_LITERAL("boundComponent")},
					Reflection::Argument{"bcde10ae-0ba6-4b0c-8dec-1f2f44e4c082"_guid, MAKE_UNICODE_LITERAL("localClient")},
					Reflection::Argument{"9e162937-d204-48ea-a6e6-106d0b5d3cac"_guid, MAKE_UNICODE_LITERAL("data")}
				},
				Function{
					"{D5184F9F-EA8A-456A-B333-3C3356269F26}"_guid,
					MAKE_UNICODE_LITERAL("Pause Gameplay"),
					&GameFramework::SceneRules::OnClientGameplayPaused,
					FunctionFlags::HostToClient,
					Reflection::ReturnType{},
					Reflection::Argument{"ec306a54-3f68-49af-94f3-bffe806b20f3"_guid, MAKE_UNICODE_LITERAL("boundComponent")},
					Reflection::Argument{"bcde10ae-0ba6-4b0c-8dec-1f2f44e4c082"_guid, MAKE_UNICODE_LITERAL("localClient")},
					Reflection::Argument{"9e162937-d204-48ea-a6e6-106d0b5d3cac"_guid, MAKE_UNICODE_LITERAL("data")}
				},
				Function{
					"{743FF7AC-765C-4E00-92E7-79020997EEF6}"_guid,
					MAKE_UNICODE_LITERAL("Resume Gameplay"),
					&GameFramework::SceneRules::OnClientGameplayResumed,
					FunctionFlags::HostToClient,
					Reflection::ReturnType{},
					Reflection::Argument{"ec306a54-3f68-49af-94f3-bffe806b20f3"_guid, MAKE_UNICODE_LITERAL("boundComponent")},
					Reflection::Argument{"bcde10ae-0ba6-4b0c-8dec-1f2f44e4c082"_guid, MAKE_UNICODE_LITERAL("localClient")},
					Reflection::Argument{"9e162937-d204-48ea-a6e6-106d0b5d3cac"_guid, MAKE_UNICODE_LITERAL("data")}
				},
				Function{
					"{047A33D7-BA55-4EB4-9DEF-BA308FA04EBA}"_guid,
					MAKE_UNICODE_LITERAL("Stop Gameplay"),
					&GameFramework::SceneRules::OnClientGameplayStopped,
					FunctionFlags::HostToClient,
					Reflection::ReturnType{},
					Reflection::Argument{"ec306a54-3f68-49af-94f3-bffe806b20f3"_guid, MAKE_UNICODE_LITERAL("boundComponent")},
					Reflection::Argument{"bcde10ae-0ba6-4b0c-8dec-1f2f44e4c082"_guid, MAKE_UNICODE_LITERAL("localClient")},
					Reflection::Argument{"9e162937-d204-48ea-a6e6-106d0b5d3cac"_guid, MAKE_UNICODE_LITERAL("data")}
				},
				Function{
					"{5360E4AC-6654-4FBD-AE48-CB9651FDE84B}"_guid,
					MAKE_UNICODE_LITERAL("Player Joined"),
					&GameFramework::SceneRules::HostOnPlayerJoined,
					FunctionFlags::ClientToHost | FunctionFlags::AllowClientToHostWithoutAuthority,
					Reflection::ReturnType{},
					Reflection::Argument{"ec306a54-3f68-49af-94f3-bffe806b20f3"_guid, MAKE_UNICODE_LITERAL("boundComponent")},
					Reflection::Argument{"ec306a54-3f68-49af-94f3-bffe806b20f3"_guid, MAKE_UNICODE_LITERAL("localHost")},
					Reflection::Argument{"1C382ED7-FE03-4E4F-AC2B-8DDEF8B4FB3B"_guid, MAKE_UNICODE_LITERAL("clientIdentifier")}
				},
				Function{
					"{861B2050-59FE-4A55-9D4A-E99CBCAF913B}"_guid,
					MAKE_UNICODE_LITERAL("Player Loaded Scene"),
					&GameFramework::SceneRules::HostOnPlayerLoadedScene,
					FunctionFlags::ClientToHost | FunctionFlags::AllowClientToHostWithoutAuthority,
					Reflection::ReturnType{},
					Reflection::Argument{"ec306a54-3f68-49af-94f3-bffe806b20f3"_guid, MAKE_UNICODE_LITERAL("boundComponent")},
					Reflection::Argument{"bcde10ae-0ba6-4b0c-8dec-1f2f44e4c082"_guid, MAKE_UNICODE_LITERAL("localHost")},
					Reflection::Argument{"1C382ED7-FE03-4E4F-AC2B-8DDEF8B4FB3B"_guid, MAKE_UNICODE_LITERAL("clientIdentifier")}
				},
				Function{
					"{a328104b-9c55-4547-8b0d-ec3e038f63cc}"_guid,
					MAKE_UNICODE_LITERAL("Player Request Restart"),
					&GameFramework::SceneRules::HostOnPlayerRequestRestart,
					FunctionFlags::ClientToHost | FunctionFlags::AllowClientToHostWithoutAuthority,
					Reflection::ReturnType{},
					Reflection::Argument{"ec306a54-3f68-49af-94f3-bffe806b20f3"_guid, MAKE_UNICODE_LITERAL("boundComponent")},
					Reflection::Argument{"bcde10ae-0ba6-4b0c-8dec-1f2f44e4c082"_guid, MAKE_UNICODE_LITERAL("localHost")},
					Reflection::Argument{"1C382ED7-FE03-4E4F-AC2B-8DDEF8B4FB3B"_guid, MAKE_UNICODE_LITERAL("clientIdentifier")}
				},
				Function{
					"78371509-e512-474c-b16d-2a5b686c05ab"_guid,
					MAKE_UNICODE_LITERAL("Player Request Pause"),
					&GameFramework::SceneRules::HostOnPlayerRequestPause,
					FunctionFlags::ClientToHost | FunctionFlags::AllowClientToHostWithoutAuthority,
					Reflection::ReturnType{},
					Reflection::Argument{"ec306a54-3f68-49af-94f3-bffe806b20f3"_guid, MAKE_UNICODE_LITERAL("boundComponent")},
					Reflection::Argument{"bcde10ae-0ba6-4b0c-8dec-1f2f44e4c082"_guid, MAKE_UNICODE_LITERAL("localHost")},
					Reflection::Argument{"1C382ED7-FE03-4E4F-AC2B-8DDEF8B4FB3B"_guid, MAKE_UNICODE_LITERAL("clientIdentifier")}
				},
				Function{
					"9b9c6ccb-a186-4ea6-8f86-1ebdab5995d8"_guid,
					MAKE_UNICODE_LITERAL("Player Request Resume"),
					&GameFramework::SceneRules::HostOnPlayerRequestResume,
					FunctionFlags::ClientToHost | FunctionFlags::AllowClientToHostWithoutAuthority,
					Reflection::ReturnType{},
					Reflection::Argument{"ec306a54-3f68-49af-94f3-bffe806b20f3"_guid, MAKE_UNICODE_LITERAL("boundComponent")},
					Reflection::Argument{"bcde10ae-0ba6-4b0c-8dec-1f2f44e4c082"_guid, MAKE_UNICODE_LITERAL("localHost")},
					Reflection::Argument{"1C382ED7-FE03-4E4F-AC2B-8DDEF8B4FB3B"_guid, MAKE_UNICODE_LITERAL("clientIdentifier")}
				},
				Function{
					"{EB9A4D91-3563-4815-96EF-3DAB83296EF3}"_guid,
					MAKE_UNICODE_LITERAL("Player Left"),
					&GameFramework::SceneRules::HostOnPlayerLeft,
					FunctionFlags::ClientToHost | FunctionFlags::AllowClientToHostWithoutAuthority,
					Reflection::ReturnType{},
					Reflection::Argument{"ec306a54-3f68-49af-94f3-bffe806b20f3"_guid, MAKE_UNICODE_LITERAL("boundComponent")},
					Reflection::Argument{"bcde10ae-0ba6-4b0c-8dec-1f2f44e4c082"_guid, MAKE_UNICODE_LITERAL("localHost")},
					Reflection::Argument{"1C382ED7-FE03-4E4F-AC2B-8DDEF8B4FB3B"_guid, MAKE_UNICODE_LITERAL("clientIdentifier")}
				}
			}
		);
	};
}
