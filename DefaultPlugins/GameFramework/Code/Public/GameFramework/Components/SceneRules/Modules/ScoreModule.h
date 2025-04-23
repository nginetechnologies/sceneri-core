#pragma once

#include <GameFramework/Components/SceneRules/Modules/Module.h>

#include <Engine/DataSource/DataSourceInterface.h>

#include <Common/Memory/UniquePtr.h>

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
	struct Score;
	struct ScoreModule;

	struct ScoreModule final : public SceneRulesModule
	{
		struct DataSource final : public ngine::DataSource::Interface
		{
			DataSource(SceneRules& sceneRules, ScoreModule& scoreModule);
			~DataSource();
			inline static constexpr Guid DataSourceGuid = "5a62fa86-fa29-45e7-8b62-fbf72e1607a3"_guid;
			// DataSource::Interface
			[[nodiscard]] virtual GenericDataIndex GetDataCount() const override;
			virtual void CacheQuery(const Query& query, CachedQuery& cachedQueryOut) const override;
			virtual void IterateData(
				const CachedQuery& query,
				IterationCallback&& callback,
				const Math::Range<GenericDataIndex> offset =
					Math::Range<GenericDataIndex>::MakeStartToEnd(0u, Math::NumericLimits<GenericDataIndex>::Max - 1u)
			) const override;
			virtual void IterateData(
				const SortedQueryIndices& query,
				IterationCallback&& callback,
				const Math::Range<GenericDataIndex> offset =
					Math::Range<GenericDataIndex>::MakeStartToEnd(0u, Math::NumericLimits<GenericDataIndex>::Max - 1u)
			) const override;
			[[nodiscard]] virtual PropertyValue GetDataProperty(const Data data, const PropertyIdentifier identifier) const override;
			// ~DataSource::Interface

			using Interface::OnDataChanged;
		private:
			DataSource::PropertyIdentifier m_scorePropertyIdentifier;
			SceneRules& m_sceneRules;
			ScoreModule& m_scoreModule;
		};

		using BaseType = SceneRulesModule;

		inline static constexpr Guid ShowScoreUIEvent = "22038124-1fc7-4b12-867c-96e9ca9d36f4"_guid;
		inline static constexpr Guid ScoreChangedUIEvent = "af809f18-0c52-441b-9c7d-ed30206914b4"_guid;

		struct Initializer : public SceneRulesModule::Initializer
		{
			using BaseType = SceneRulesModule::Initializer;
			using BaseType::BaseType;

			Initializer(BaseType&& initializer)
				: BaseType(Forward<SceneRulesModule::Initializer>(initializer))
			{
			}
		};

		ScoreModule(const ScoreModule& templateComponent, const Cloner& cloner);
		ScoreModule(const Deserializer& deserializer);
		ScoreModule(Initializer&& initializer);
		virtual ~ScoreModule();

		virtual void OnGameplayStarted(SceneRules&) override;
		virtual void OnGameplayStopped(SceneRules&) override;

		void AddScore(SceneRules&, const ClientIdentifier clientIdentifier, const int32 value);
		[[nodiscard]] int32 GetScore(SceneRules&, const ClientIdentifier clientIdentifier);
	protected:
		void OnPlayerSpawned(SceneRules&, const ClientIdentifier, Entity::Component3D&);
		void OnPlayerHUDLoaded(SceneRules&, const ClientIdentifier);

		void EnableUI(SceneRules&, const ClientIdentifier);

		void SetScoreInternal(SceneRules& sceneRules, Score& playerScore, const ClientIdentifier clientIdentifier, const int32 newScore);

		friend struct Reflection::ReflectedType<ScoreModule>;
		struct ScoreChangedData
		{
			ClientIdentifier playerIdentifier;
			int32 score;
		};
		friend struct Reflection::ReflectedType<ScoreChangedData>;
		void ClientOnPlayerScoreChanged(
			Entity::HierarchyComponentBase& parent,
			Network::Session::BoundComponent& boundComponent,
			Network::LocalClient& localClient,
			const ScoreChangedData data
		);
	private:
		UniquePtr<DataSource> m_pDataSource;
	};

}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<GameFramework::ScoreModule::ScoreChangedData>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::ScoreModule::ScoreChangedData>(
			"{5E0D86F0-3F96-4D74-896E-C54FEDD72C06}"_guid,
			MAKE_UNICODE_LITERAL("Player Score Changed Data"),
			TypeFlags{},
			Tags{},
			Properties{
				Property{
					MAKE_UNICODE_LITERAL("Player Identifier"),
					"playerIdentifier",
					"{2264741C-D19E-48AE-827D-33AF61D322F3}"_guid,
					MAKE_UNICODE_LITERAL("PlayerSpawnedData"),
					PropertyFlags::SentWithNetworkedFunctions,
					&GameFramework::ScoreModule::ScoreChangedData::playerIdentifier
				},
				Property{
					MAKE_UNICODE_LITERAL("Health"),
					"score",
					"{AEBB4020-AD97-4FAE-8522-8B9ECF3C45CF}"_guid,
					MAKE_UNICODE_LITERAL("PlayerSpawnedData"),
					PropertyFlags::SentWithNetworkedFunctions,
					&GameFramework::ScoreModule::ScoreChangedData::score
				}
			}
		);
	};

	template<>
	struct ReflectedType<GameFramework::ScoreModule>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::ScoreModule>(
			"0eac250f-0917-43ea-871f-b6ae73ececf9"_guid,
			MAKE_UNICODE_LITERAL("Score Game Rules Module"),
			TypeFlags{},
			Tags{},
			Properties{},
			Functions{Function{
				"{865F75D5-3C4D-4EEE-9D6C-1DD1D1775961}"_guid,
				MAKE_UNICODE_LITERAL("On Player Score Changed"),
				&GameFramework::ScoreModule::ClientOnPlayerScoreChanged,
				FunctionFlags::HostToClient,
				Reflection::ReturnType{},
				Reflection::Argument{"{F1E5E1DE-99FA-44DB-A0D6-52E5B78D4472}"_guid, MAKE_UNICODE_LITERAL("owner")},
				Reflection::Argument{"ec306a54-3f68-49af-94f3-bffe806b20f3"_guid, MAKE_UNICODE_LITERAL("boundComponent")},
				Reflection::Argument{"bcde10ae-0ba6-4b0c-8dec-1f2f44e4c082"_guid, MAKE_UNICODE_LITERAL("localClient")},
				Reflection::Argument{"9e162937-d204-48ea-a6e6-106d0b5d3cac"_guid, MAKE_UNICODE_LITERAL("data")}
			}}
		);
	};
}
