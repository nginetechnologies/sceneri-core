#pragma once

#include <GameFramework/Components/SceneRules/Modules/Module.h>
#include <GameFramework/Components/SceneRules/FinishResult.h>

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
	//! Module that implements the concept of players being able to reach a final goal which ends the game for them
	struct FinishModule final : SceneRulesModule
	{
		using BaseType = SceneRulesModule;

		static constexpr Guid PlayerFinishSuccessEvent = "a750198d-7f59-4ed0-8d22-441b56e19b60"_guid;
		static constexpr Guid PlayerFinishFailEvent = "6586a3cf-7f11-4565-8124-4947b40a79bf"_guid;

		FinishModule(const FinishModule& templateComponent, const Cloner& cloner);
		FinishModule(const Deserializer& deserializer);
		FinishModule(Initializer&& initializer);

		virtual void OnGameplayStarted(SceneRules&) override;
		virtual void OnGameplayStopped(SceneRules&) override;
		virtual void HostOnPlayerSceneLoaded(SceneRules&, const ClientIdentifier) override;
		virtual void HostOnPlayerLeft(SceneRules&, const ClientIdentifier) override;

		using PlayerMask = IdentifierMask<ClientIdentifier>;
		using AtomicPlayerMask = Threading::AtomicIdentifierMask<ClientIdentifier>;

		[[nodiscard]] bool HasPlayerFinished(const ClientIdentifier playerIdentifier) const
		{
			return !m_activePlayers.IsSet(playerIdentifier);
		}
		//! Gets an iterator to players that that haven't reached the finish condition yet
		[[nodiscard]] AtomicPlayerMask::SetBitsIterator GetActivePlayerIterator() const
		{
			return m_activePlayers.GetSetBitsIterator();
		}

		Event<void(void*, SceneRules&, const ClientIdentifier, GameRulesFinishResult), 24> OnPlayerFinished;

		void NotifyPlayerFinished(SceneRules&, const ClientIdentifier playerIdentifier, const GameRulesFinishResult finishResult);
		void FinishAllRemainingPlayers(SceneRules&, const GameRulesFinishResult finishResult);
	protected:
		friend struct Reflection::ReflectedType<FinishModule>;

		void OnPlayerSpawned(SceneRules& sceneRules, const ClientIdentifier playerIdentifier, Entity::Component3D& playerComponent);

		void ClientOnPlayerFinishedSuccess(
			Entity::HierarchyComponentBase& parent,
			Network::Session::BoundComponent& boundComponent,
			Network::LocalClient& localClient,
			const ClientIdentifier clientIdentifier
		);
		void ClientOnPlayerFinishedSuccessInternal(SceneRules& sceneRules, const ClientIdentifier playerIdentifier);
		void ClientOnPlayerFinishedFailure(
			Entity::HierarchyComponentBase& parent,
			Network::Session::BoundComponent& boundComponent,
			Network::LocalClient& localClient,
			const ClientIdentifier clientIdentifier
		);
		void ClientOnPlayerFinishedFailureInternal(SceneRules& sceneRules, const ClientIdentifier playerIdentifier);
	private:
		//! Players that haven't reached the finish condition yet
		AtomicPlayerMask m_activePlayers;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<GameFramework::FinishModule>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::FinishModule>(
			"2ccaab2c-f671-481e-b0e3-a95106c62da6"_guid,
			MAKE_UNICODE_LITERAL("Finish Game Rules Module"),
			TypeFlags{},
			Tags{},
			Properties{},
			Functions{
				Function{
					"{CAEA9347-922A-4003-8705-C7214C3D854A}"_guid,
					MAKE_UNICODE_LITERAL("Player Finished Success"),
					&GameFramework::FinishModule::ClientOnPlayerFinishedSuccess,
					FunctionFlags::HostToClient,
					Reflection::ReturnType{},
					Reflection::Argument{"{F1E5E1DE-99FA-44DB-A0D6-52E5B78D4472}"_guid, MAKE_UNICODE_LITERAL("owner")},
					Reflection::Argument{"ec306a54-3f68-49af-94f3-bffe806b20f3"_guid, MAKE_UNICODE_LITERAL("boundComponent")},
					Reflection::Argument{"bcde10ae-0ba6-4b0c-8dec-1f2f44e4c082"_guid, MAKE_UNICODE_LITERAL("localClient")},
					Reflection::Argument{"9e162937-d204-48ea-a6e6-106d0b5d3cac"_guid, MAKE_UNICODE_LITERAL("data")}
				},
				Function{
					"{594E260B-BDD6-4E33-BE84-46667A211F06}"_guid,
					MAKE_UNICODE_LITERAL("Player Finish Failure"),
					&GameFramework::FinishModule::ClientOnPlayerFinishedFailure,
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
