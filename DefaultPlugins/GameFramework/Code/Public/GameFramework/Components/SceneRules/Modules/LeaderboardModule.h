#pragma once

#include <GameFramework/Components/SceneRules/Modules/Module.h>

#include <Engine/DataSource/PropertySourceInterface.h>

#include <Common/Memory/UniquePtr.h>
#include <Common/Reflection/CoreTypes.h>
#include <Common/Function/Event.h>

namespace ngine::Networking::Backend
{
	struct Leaderboard;
}

namespace ngine::GameFramework
{
	struct Score;

	struct LeaderboardModule final : public SceneRulesModule
	{
		using BaseType = SceneRulesModule;

		struct Initializer : public SceneRulesModule::Initializer
		{
			using BaseType = SceneRulesModule::Initializer;
			using BaseType::BaseType;

			Initializer(BaseType&& initializer)
				: BaseType(Forward<SceneRulesModule::Initializer>(initializer))
			{
			}
		};

		LeaderboardModule(const LeaderboardModule& templateComponent, const Cloner& cloner);
		LeaderboardModule(const Deserializer& deserializer);
		LeaderboardModule(Initializer&& initializer);
		virtual ~LeaderboardModule();

		void OnParentCreated(SceneRules& sceneRules);
	protected:
		void OnPlayerFinishedInternal(SceneRules&, const ClientIdentifier, const GameRulesFinishResult);
	protected:
		friend struct Reflection::ReflectedType<LeaderboardModule>;

		UniquePtr<Networking::Backend::Leaderboard> m_pLeaderboard;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<GameFramework::LeaderboardModule>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::LeaderboardModule>(
			"a45076cb-ff23-4203-b378-c9c1e4eb98a6"_guid, MAKE_UNICODE_LITERAL("Leaderboard Game Rules Module")
		);
	};
}
