#include "Components/SceneRules/SceneRules.h"
#include "Components/SceneRules/Modules/Module.h"
#include "Components/SceneRules/Modules/FinishModule.h"

#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/RootSceneComponent.h>
#include <Engine/Entity/Component3D.h>
#include <Engine/Entity/Data/Component.h>
#include <Engine/Entity/ComponentTypeSceneDataInterface.h>
#include <Engine/Entity/Scene/SceneComponent.h>
#include <Engine/Entity/RootSceneComponent.h>
#include <Engine/Threading/JobManager.h>
#include <Engine/Input/Devices/Gamepad/Gamepad.h>
#include <Engine/Event/EventManager.h>
#include <Engine/Entity/ComponentSoftReference.inl>
#include <Engine/Entity/Component3D.inl>

#include <Renderer/Renderer.h>

#include <GameFramework/Components/Player/Player.h>
#include <GameFramework/Components/Player/Score.h>
#include <GameFramework/Plugin.h>

#include <NetworkingCore/Components/HostComponent.h>
#include <NetworkingCore/Components/LocalHostComponent.h>
#include <NetworkingCore/Components/ClientComponent.h>
#include <NetworkingCore/Components/LocalClientComponent.h>
#include <NetworkingCore/Components/BoundComponent.h>
#include <Common/Threading/Jobs/AsyncJob.h>
#include <Common/System/Query.h>
#include <Common/Reflection/Registry.inl>
#include <Common/Threading/Jobs/JobRunnerThread.inl>

namespace ngine::GameFramework
{
	[[nodiscard]] bool IsLocalHost(Entity::HierarchyComponentBase& rootParent, Entity::SceneRegistry& sceneRegistry)
	{
		return Network::Session::LocalHost::Find(rootParent, sceneRegistry).IsValid();
	}

	Optional<SceneRules*> SceneRules::Find(Entity::HierarchyComponentBase& rootSceneComponent)
	{
		return rootSceneComponent.FindFirstChildImplementingTypeRecursive<SceneRules>();
	}

	SceneRules::SceneRules(const SceneRules& component, const Cloner& cloner)
		: Entity::Component3D(component, cloner)
		, m_flags(Flags::IsHost * IsLocalHost(GetRootParent(), cloner.GetSceneRegistry()))
	{
		CreateDataComponent<Network::Session::BoundComponent>(
			cloner.GetParent()->GetSceneRegistry(),
			Network::Session::BoundComponent::Initializer{*this, cloner.GetParent()->GetSceneRegistry()}
		);
	}

	SceneRules::SceneRules(const Deserializer& deserializer)
		: Entity::Component3D(deserializer)
	{
	}

	SceneRules::SceneRules(Initializer&& initializer)
		: Entity::Component3D(Forward<Initializer>(initializer))
		, m_flags(Flags::IsHost * IsLocalHost(GetRootParent(), initializer.GetSceneRegistry()))
	{
		CreateDataComponent<Network::Session::BoundComponent>(
			initializer.GetSceneRegistry(),
			Network::Session::BoundComponent::Initializer{*this, initializer.GetSceneRegistry()}
		);
	}

	bool SceneRules::Start()
	{
		Assert(m_flags.IsSet(Flags::IsHost));
		if (LIKELY(m_flags.IsSet(Flags::IsHost)))
		{
			const EnumFlags<Flags> previousFlags = m_flags.FetchOr(Flags::HasGameplayStarted);
			Assert(previousFlags.IsNotSet(Flags::HasGameplayStarted), "Can't start scene rules that was already started!");
			if (previousFlags.IsNotSet(Flags::HasGameplayStarted))
			{
				OnGameplayStartedInternal();

				const Optional<Network::Session::BoundComponent*> pBoundComponent = FindDataComponentOfType<Network::Session::BoundComponent>();
				Assert(pBoundComponent.IsValid());
				if (LIKELY(pBoundComponent.IsValid()))
				{
					for (const ClientIdentifier::IndexType clientIndex : m_loadedPlayers.GetSetBitsIterator())
					{
						const ClientIdentifier clientIdentifier = ClientIdentifier::MakeFromValidIndex(clientIndex);
						pBoundComponent->SendMessageToClient<&SceneRules::OnClientGameplayStarted>(
							*this,
							GetSceneRegistry(),
							clientIdentifier,
							Network::Channel{0},
							GameplayStartedData{}
						);
					}
				}
				return true;
			}
		}
		return false;
	}

	void SceneRules::OnClientGameplayStarted(Network::Session::BoundComponent&, Network::LocalClient&, const GameplayStartedData)
	{
		const EnumFlags<Flags> previousFlags = m_flags.FetchOr(Flags::HasGameplayStarted);
		if (previousFlags.IsNotSet(Flags::HasGameplayStarted))
		{
			OnGameplayStartedInternal();
		}
	}

	void SceneRules::OnGameplayStartedInternal()
	{
		IterateDataComponentsImplementingType<SceneRulesModule>(
			GetSceneRegistry(),
			[this](Entity::Data::Component& dataComponent, const Optional<const Entity::ComponentTypeInterface*>, Entity::ComponentTypeSceneDataInterface&)
			{
				SceneRulesModule& module = static_cast<SceneRulesModule&>(dataComponent);
				module.OnGameplayStarted(*this);
				return Memory::CallbackResult::Continue;
			}
		);
	}

	bool SceneRules::Pause()
	{
		Assert(m_flags.IsSet(Flags::IsHost));
		if (LIKELY(m_flags.IsSet(Flags::IsHost)))
		{
			const EnumFlags<Flags> previousFlags = m_flags.FetchOr(Flags::IsGameplayPaused);
			if (previousFlags.IsNotSet(Flags::IsGameplayPaused))
			{
				OnGameplayPausedInternal();

				const Optional<Network::Session::BoundComponent*> pBoundComponent = FindDataComponentOfType<Network::Session::BoundComponent>();
				Assert(pBoundComponent.IsValid());
				if (LIKELY(pBoundComponent.IsValid()))
				{
					for (const ClientIdentifier::IndexType clientIndex : m_loadedPlayers.GetSetBitsIterator())
					{
						const ClientIdentifier clientIdentifier = ClientIdentifier::MakeFromValidIndex(clientIndex);
						pBoundComponent->SendMessageToClient<&SceneRules::OnClientGameplayPaused>(
							*this,
							GetSceneRegistry(),
							clientIdentifier,
							Network::Channel{0},
							GameplayPausedData{}
						);
					}
				}

				return true;
			}
		}
		return false;
	}

	void SceneRules::OnClientGameplayPaused(Network::Session::BoundComponent&, Network::LocalClient&, const GameplayPausedData)
	{
		const EnumFlags<Flags> previousFlags = m_flags.FetchOr(Flags::IsGameplayPaused);
		if (previousFlags.IsNotSet(Flags::IsGameplayPaused))
		{
			OnGameplayPausedInternal();
		}
	}

	void SceneRules::OnGameplayPausedInternal()
	{
		IterateDataComponentsImplementingType<SceneRulesModule>(
			GetSceneRegistry(),
			[this](Entity::Data::Component& dataComponent, const Optional<const Entity::ComponentTypeInterface*>, Entity::ComponentTypeSceneDataInterface&)
			{
				SceneRulesModule& module = static_cast<SceneRulesModule&>(dataComponent);
				module.OnGameplayPaused(*this);
				return Memory::CallbackResult::Continue;
			}
		);
	}

	bool SceneRules::Resume()
	{
		Assert(m_flags.IsSet(Flags::IsHost));
		if (LIKELY(m_flags.IsSet(Flags::IsHost)))
		{
			const EnumFlags<Flags> previousFlags = m_flags.FetchAnd(~Flags::IsGameplayPaused);
			if (previousFlags.IsSet(Flags::IsGameplayPaused))
			{
				OnGameplayResumedInternal();

				const Optional<Network::Session::BoundComponent*> pBoundComponent = FindDataComponentOfType<Network::Session::BoundComponent>();
				Assert(pBoundComponent.IsValid());
				if (LIKELY(pBoundComponent.IsValid()))
				{
					pBoundComponent->BroadcastToAllClients<&SceneRules::OnClientGameplayResumed>(
						*this,
						GetSceneRegistry(),
						Network::Channel{0},
						GameplayResumedData{}
					);
				}

				return true;
			}
		}
		return false;
	}

	void SceneRules::OnClientGameplayResumed(Network::Session::BoundComponent&, Network::LocalClient&, const GameplayResumedData)
	{
		const EnumFlags<Flags> previousFlags = m_flags.FetchAnd(~Flags::IsGameplayPaused);
		if (previousFlags.IsSet(Flags::IsGameplayPaused))
		{
			OnGameplayResumedInternal();
		}
	}

	void SceneRules::OnGameplayResumedInternal()
	{
		IterateDataComponentsImplementingType<SceneRulesModule>(
			GetSceneRegistry(),
			[this](Entity::Data::Component& dataComponent, const Optional<const Entity::ComponentTypeInterface*>, Entity::ComponentTypeSceneDataInterface&)
			{
				SceneRulesModule& module = static_cast<SceneRulesModule&>(dataComponent);
				module.OnGameplayResumed(*this);
				return Memory::CallbackResult::Continue;
			}
		);
	}

	bool SceneRules::Restart()
	{
		Assert(m_flags.IsSet(Flags::IsHost));
		if (LIKELY(m_flags.IsSet(Flags::IsHost)))
		{
			if (Stop())
			{
				return Start();
			}
		}
		return false;
	}

	bool SceneRules::Stop()
	{
		Assert(m_flags.IsSet(Flags::IsHost));
		if (LIKELY(m_flags.IsSet(Flags::IsHost)))
		{
			const EnumFlags<Flags> previousFlags = m_flags.FetchAnd(~(Flags::HasGameplayStarted | Flags::IsGameplayPaused));
			Assert(previousFlags.IsSet(Flags::HasGameplayStarted), "Can't stop scene rules that hasn't started yet!");
			if (previousFlags.IsSet(Flags::HasGameplayStarted))
			{
				OnGameplayStoppedInternal();

				const Optional<Network::Session::BoundComponent*> pBoundComponent = FindDataComponentOfType<Network::Session::BoundComponent>();
				Assert(pBoundComponent.IsValid());
				if (LIKELY(pBoundComponent.IsValid()))
				{
					for (const ClientIdentifier::IndexType clientIndex : m_loadedPlayers.GetSetBitsIterator())
					{
						const ClientIdentifier clientIdentifier = ClientIdentifier::MakeFromValidIndex(clientIndex);
						pBoundComponent->SendMessageToClient<&SceneRules::OnClientGameplayStopped>(
							*this,
							GetSceneRegistry(),
							clientIdentifier,
							Network::Channel{0},
							GameplayStoppedData{}
						);
					}
				}

				return true;
			}
		}
		return false;
	}

	void SceneRules::OnClientGameplayStopped(Network::Session::BoundComponent&, Network::LocalClient&, const GameplayStoppedData)
	{
		const EnumFlags<Flags> previousFlags = m_flags.FetchAnd(~(Flags::HasGameplayStarted | Flags::IsGameplayPaused));
		if (previousFlags.IsSet(Flags::HasGameplayStarted))
		{
			OnGameplayStoppedInternal();
		}
	}

	void SceneRules::OnGameplayStoppedInternal()
	{
		IterateDataComponentsImplementingType<SceneRulesModule>(
			GetSceneRegistry(),
			[this](Entity::Data::Component& dataComponent, const Optional<const Entity::ComponentTypeInterface*>, Entity::ComponentTypeSceneDataInterface&)
			{
				SceneRulesModule& module = static_cast<SceneRulesModule&>(dataComponent);
				module.OnGameplayStopped(*this);
				return Memory::CallbackResult::Continue;
			}
		);
	}

	void SceneRules::OnLocalPlayerJoined(const ClientIdentifier playerIdentifier)
	{
		Assert(playerIdentifier.IsValid());
		const bool wasSet = m_localPlayers.Set(playerIdentifier);
		Assert(wasSet, "Local player had already joined!");
		if (LIKELY(wasSet))
		{
			if (m_flags.IsSet(Flags::IsHost))
			{
				HostOnPlayerJoinedInternal(playerIdentifier);
			}
			else
			{
				const Optional<Network::Session::BoundComponent*> pBoundComponent = FindDataComponentOfType<Network::Session::BoundComponent>();
				Assert(pBoundComponent.IsValid());
				if (LIKELY(pBoundComponent.IsValid()))
				{
					pBoundComponent->SendMessageToHost<&SceneRules::HostOnPlayerJoined>(*this, GetSceneRegistry(), Network::Channel{0});
				}
			}

			IterateDataComponentsImplementingType<SceneRulesModule>(
				GetSceneRegistry(),
				[this,
			   playerIdentifier](Entity::Data::Component& dataComponent, const Optional<const Entity::ComponentTypeInterface*>, Entity::ComponentTypeSceneDataInterface&)
				{
					SceneRulesModule& module = static_cast<SceneRulesModule&>(dataComponent);
					module.ClientOnLocalPlayerJoined(*this, playerIdentifier);
					return Memory::CallbackResult::Continue;
				}
			);
		}
	}

	void SceneRules::HostOnPlayerJoined(Network::Session::BoundComponent&, Network::LocalClient&, const ClientIdentifier clientIdentifier)
	{
		HostOnPlayerJoinedInternal(clientIdentifier);
	}

	void SceneRules::HostOnPlayerJoinedInternal(const ClientIdentifier playerIdentifier)
	{
		const bool joined = m_players.Set(playerIdentifier);
		Assert(joined);
		if (LIKELY(joined))
		{
			if (m_flags.IsNotSet(Flags::HasGameplayStarted))
			{
				Start();
			}

			Assert(m_flags.IsSet(Flags::IsHost));
			IterateDataComponentsImplementingType<SceneRulesModule>(
				GetSceneRegistry(),
				[this,
			   playerIdentifier](Entity::Data::Component& dataComponent, const Optional<const Entity::ComponentTypeInterface*>, Entity::ComponentTypeSceneDataInterface&)
				{
					SceneRulesModule& module = static_cast<SceneRulesModule&>(dataComponent);
					module.HostOnPlayerJoined(*this, playerIdentifier);
					return Memory::CallbackResult::Continue;
				}
			);
		}
	}

	void SceneRules::OnLocalPlayerLoadedScene(const ClientIdentifier playerIdentifier)
	{
		if (m_localPlayers.IsNotSet(playerIdentifier))
		{
			OnLocalPlayerJoined(playerIdentifier);
		}

		IterateDataComponentsImplementingType<SceneRulesModule>(
			GetSceneRegistry(),
			[this,
		   playerIdentifier](Entity::Data::Component& dataComponent, const Optional<const Entity::ComponentTypeInterface*>, Entity::ComponentTypeSceneDataInterface&)
			{
				SceneRulesModule& module = static_cast<SceneRulesModule&>(dataComponent);
				module.OnLocalSceneLoaded(*this, playerIdentifier);
				return Memory::CallbackResult::Continue;
			}
		);

		if (m_flags.IsSet(Flags::IsHost))
		{
			HostOnPlayerLoadedSceneInternal(playerIdentifier);
		}
		else
		{
			const Optional<Network::Session::BoundComponent*> pBoundComponent = FindDataComponentOfType<Network::Session::BoundComponent>();
			Assert(pBoundComponent.IsValid());
			if (LIKELY(pBoundComponent.IsValid()))
			{
				pBoundComponent->SendMessageToHost<&SceneRules::HostOnPlayerLoadedScene>(*this, GetSceneRegistry(), Network::Channel{0});
			}
		}
	}

	void
	SceneRules::HostOnPlayerLoadedScene(Network::Session::BoundComponent&, Network::LocalClient&, const ClientIdentifier clientIdentifier)
	{
		HostOnPlayerLoadedSceneInternal(clientIdentifier);
	}

	void SceneRules::HostOnPlayerLoadedSceneInternal(const ClientIdentifier playerIdentifier)
	{
		Assert(m_flags.IsSet(Flags::IsHost));
		const bool wasSet = m_loadedPlayers.Set(playerIdentifier);
		Assert(wasSet);
		if (LIKELY(wasSet))
		{
			if (m_flags.IsSet(Flags::HasGameplayStarted))
			{
				const Optional<Network::Session::BoundComponent*> pBoundComponent = FindDataComponentOfType<Network::Session::BoundComponent>();
				Assert(pBoundComponent.IsValid());
				if (LIKELY(pBoundComponent.IsValid()))
				{
					pBoundComponent->SendMessageToClient<&SceneRules::OnClientGameplayStarted>(
						*this,
						GetSceneRegistry(),
						playerIdentifier,
						Network::Channel{0},
						GameplayStartedData{}
					);
				}
			}

			IterateDataComponentsImplementingType<SceneRulesModule>(
				GetSceneRegistry(),
				[this,
			   playerIdentifier](Entity::Data::Component& dataComponent, const Optional<const Entity::ComponentTypeInterface*>, Entity::ComponentTypeSceneDataInterface&)
				{
					SceneRulesModule& module = static_cast<SceneRulesModule&>(dataComponent);
					module.HostOnPlayerSceneLoaded(*this, playerIdentifier);
					return Memory::CallbackResult::Continue;
				}
			);
		}
	}

	void SceneRules::OnLocalPlayerLeft(const ClientIdentifier playerIdentifier)
	{
		Assert(playerIdentifier.IsValid());
		const bool wasCleared = m_localPlayers.Clear(playerIdentifier);
		Assert(wasCleared, "Local player was not participating!");
		if (LIKELY(wasCleared))
		{
			m_loadedPlayers.Clear(playerIdentifier);

			if (m_flags.IsSet(Flags::IsHost))
			{
				HostOnPlayerLeftInternal(playerIdentifier);
			}
			else
			{
				const Optional<Network::Session::BoundComponent*> pBoundComponent = FindDataComponentOfType<Network::Session::BoundComponent>();
				Assert(pBoundComponent.IsValid());
				if (LIKELY(pBoundComponent.IsValid()))
				{
					pBoundComponent->SendMessageToHost<&SceneRules::HostOnPlayerLeft>(*this, GetSceneRegistry(), Network::Channel{0});
				}
			}

			IterateDataComponentsImplementingType<SceneRulesModule>(
				GetSceneRegistry(),
				[this,
			   playerIdentifier](Entity::Data::Component& dataComponent, const Optional<const Entity::ComponentTypeInterface*>, Entity::ComponentTypeSceneDataInterface&)
				{
					SceneRulesModule& module = static_cast<SceneRulesModule&>(dataComponent);
					module.ClientOnLocalPlayerLeft(*this, playerIdentifier);
					return Memory::CallbackResult::Continue;
				}
			);
		}
	}

	void SceneRules::HostOnPlayerLeft(Network::Session::BoundComponent&, Network::LocalClient&, const ClientIdentifier clientIdentifier)
	{
		HostOnPlayerLeftInternal(clientIdentifier);
	}

	void SceneRules::HostOnPlayerLeftInternal(const ClientIdentifier playerIdentifier)
	{
		const bool left = m_players.Clear(playerIdentifier);
		Assert(left);
		if (LIKELY(left))
		{
			if (m_flags.IsSet(Flags::HasGameplayStarted) && !m_players.AreAnySet())
			{
				Stop();
			}

			Assert(m_flags.IsSet(Flags::IsHost));
			IterateDataComponentsImplementingType<SceneRulesModule>(
				GetSceneRegistry(),
				[this,
			   playerIdentifier](Entity::Data::Component& dataComponent, const Optional<const Entity::ComponentTypeInterface*>, Entity::ComponentTypeSceneDataInterface&)
				{
					SceneRulesModule& module = static_cast<SceneRulesModule&>(dataComponent);
					module.HostOnPlayerLeft(*this, playerIdentifier);
					return Memory::CallbackResult::Continue;
				}
			);
		}
	}

	void SceneRules::OnLocalPlayerRequestRestart(const ClientIdentifier playerIdentifier)
	{
		if (m_flags.IsSet(Flags::IsHost))
		{
			HostOnPlayerRequestRestartInternal(playerIdentifier);
		}
		else
		{
			const Optional<Network::Session::BoundComponent*> pBoundComponent = FindDataComponentOfType<Network::Session::BoundComponent>();
			Assert(pBoundComponent.IsValid());
			if (LIKELY(pBoundComponent.IsValid()))
			{
				pBoundComponent->SendMessageToHost<&SceneRules::HostOnPlayerRequestRestart>(*this, GetSceneRegistry(), Network::Channel{0});
			}
		}
	}

	void
	SceneRules::HostOnPlayerRequestRestart(Network::Session::BoundComponent&, Network::LocalClient&, const ClientIdentifier clientIdentifier)
	{
		HostOnPlayerRequestRestartInternal(clientIdentifier);
	}

	void SceneRules::HostOnPlayerRequestRestartInternal(const ClientIdentifier playerIdentifier)
	{
		Assert(m_flags.IsSet(Flags::IsHost));
		IterateDataComponentsImplementingType<SceneRulesModule>(
			GetSceneRegistry(),
			[this,
		   playerIdentifier](Entity::Data::Component& dataComponent, const Optional<const Entity::ComponentTypeInterface*>, Entity::ComponentTypeSceneDataInterface&)
			{
				SceneRulesModule& module = static_cast<SceneRulesModule&>(dataComponent);
				module.HostOnPlayerRequestRestart(*this, playerIdentifier);
				return Memory::CallbackResult::Continue;
			}
		);
	}

	void SceneRules::OnLocalPlayerRequestPause(const ClientIdentifier playerIdentifier)
	{
		IterateDataComponentsImplementingType<SceneRulesModule>(
			GetSceneRegistry(),
			[this,
		   playerIdentifier](Entity::Data::Component& dataComponent, const Optional<const Entity::ComponentTypeInterface*>, Entity::ComponentTypeSceneDataInterface&)
			{
				SceneRulesModule& module = static_cast<SceneRulesModule&>(dataComponent);
				module.OnLocalPlayerPaused(*this, playerIdentifier);
				return Memory::CallbackResult::Continue;
			}
		);

		if (m_flags.IsSet(Flags::IsHost))
		{
			HostOnPlayerRequestPauseInternal(playerIdentifier);
		}
		else
		{
			const Optional<Network::Session::BoundComponent*> pBoundComponent = FindDataComponentOfType<Network::Session::BoundComponent>();
			Assert(pBoundComponent.IsValid());
			if (LIKELY(pBoundComponent.IsValid()))
			{
				pBoundComponent->SendMessageToHost<&SceneRules::HostOnPlayerRequestPause>(*this, GetSceneRegistry(), Network::Channel{0});
			}
		}
	}

	void
	SceneRules::HostOnPlayerRequestPause(Network::Session::BoundComponent&, Network::LocalClient&, const ClientIdentifier clientIdentifier)
	{
		HostOnPlayerRequestPauseInternal(clientIdentifier);
	}

	void SceneRules::HostOnPlayerRequestPauseInternal(const ClientIdentifier playerIdentifier)
	{
		Assert(m_flags.IsSet(Flags::IsHost));
		IterateDataComponentsImplementingType<SceneRulesModule>(
			GetSceneRegistry(),
			[this,
		   playerIdentifier](Entity::Data::Component& dataComponent, const Optional<const Entity::ComponentTypeInterface*>, Entity::ComponentTypeSceneDataInterface&)
			{
				SceneRulesModule& module = static_cast<SceneRulesModule&>(dataComponent);
				module.HostOnPlayerRequestPause(*this, playerIdentifier);
				return Memory::CallbackResult::Continue;
			}
		);
	}

	void SceneRules::OnLocalPlayerRequestResume(const ClientIdentifier playerIdentifier)
	{
		IterateDataComponentsImplementingType<SceneRulesModule>(
			GetSceneRegistry(),
			[this,
		   playerIdentifier](Entity::Data::Component& dataComponent, const Optional<const Entity::ComponentTypeInterface*>, Entity::ComponentTypeSceneDataInterface&)
			{
				SceneRulesModule& module = static_cast<SceneRulesModule&>(dataComponent);
				module.OnLocalPlayerResumed(*this, playerIdentifier);
				return Memory::CallbackResult::Continue;
			}
		);

		if (m_flags.IsSet(Flags::IsHost))
		{
			HostOnPlayerRequestResumeInternal(playerIdentifier);
		}
		else
		{
			const Optional<Network::Session::BoundComponent*> pBoundComponent = FindDataComponentOfType<Network::Session::BoundComponent>();
			Assert(pBoundComponent.IsValid());
			if (LIKELY(pBoundComponent.IsValid()))
			{
				pBoundComponent->SendMessageToHost<&SceneRules::HostOnPlayerRequestPause>(*this, GetSceneRegistry(), Network::Channel{0});
			}
		}
	}

	void
	SceneRules::HostOnPlayerRequestResume(Network::Session::BoundComponent&, Network::LocalClient&, const ClientIdentifier clientIdentifier)
	{
		HostOnPlayerRequestResumeInternal(clientIdentifier);
	}

	void SceneRules::HostOnPlayerRequestResumeInternal(const ClientIdentifier playerIdentifier)
	{
		Assert(m_flags.IsSet(Flags::IsHost));
		IterateDataComponentsImplementingType<SceneRulesModule>(
			GetSceneRegistry(),
			[this,
		   playerIdentifier](Entity::Data::Component& dataComponent, const Optional<const Entity::ComponentTypeInterface*>, Entity::ComponentTypeSceneDataInterface&)
			{
				SceneRulesModule& module = static_cast<SceneRulesModule&>(dataComponent);
				module.HostOnPlayerRequestResume(*this, playerIdentifier);
				return Memory::CallbackResult::Continue;
			}
		);
	}

	[[maybe_unused]] const bool wasSceneRulesRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<SceneRules>>::Make());
	[[maybe_unused]] const bool wasSceneRulesTypeRegistered = Reflection::Registry::RegisterType<SceneRules>();

	[[maybe_unused]] const bool wasModuleComponentTypeRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<SceneRulesModule>>::Make());
	[[maybe_unused]] const bool wasModuleTypeRegistered = Reflection::Registry::RegisterType<SceneRulesModule>();

	// Helper to serialize old game rules / lifecycle before we made scene rules more generic
	// Automatically adds finish game support that used to be a native concept
	struct LegacyGameRules final : public SceneRules
	{
		using BaseType = SceneRules;
		LegacyGameRules(const SceneRules& templateComponent, const Cloner& cloner)
			: SceneRules(templateComponent, cloner)
		{
		}
		LegacyGameRules(const Deserializer& deserializer)
			: SceneRules(deserializer)
		{
			CreateDataComponent<FinishModule>(FinishModule::Initializer{*this, deserializer.GetSceneRegistry()});
		}
	};

	[[maybe_unused]] const bool wasLegacyGameRulesComponentTypeRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<LegacyGameRules>>::Make());
	[[maybe_unused]] const bool wasLegacyGameRulesTypeRegistered = Reflection::Registry::RegisterType<LegacyGameRules>();
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<GameFramework::LegacyGameRules>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::LegacyGameRules>(
			"d2c2c3a5-b813-4ab8-b52f-7986940e5b67"_guid,
			MAKE_UNICODE_LITERAL("Legacy Game Rules"),
			Reflection::TypeFlags::DisableUserInterfaceInstantiation | Reflection::TypeFlags::DisableDynamicInstantiation
		);
	};
}

namespace ngine::GameFramework
{
	[[maybe_unused]] const bool wasPlayerRegistered = Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<Player>>::Make());
	[[maybe_unused]] const bool wasPlayerTypeRegistered = Reflection::Registry::RegisterType<Player>();

	[[maybe_unused]] const bool wasScoreRegistered = Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<Score>>::Make());
	[[maybe_unused]] const bool wasScoreTypeRegistered = Reflection::Registry::RegisterType<Score>();
}
