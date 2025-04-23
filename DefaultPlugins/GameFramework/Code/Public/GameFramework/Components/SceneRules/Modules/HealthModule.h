#pragma once

#include <Common/Asset/Guid.h>

#include <GameFramework/Components/SceneRules/Modules/Module.h>

#include <Engine/DataSource/DataSourceInterface.h>

#include <Common/Memory/UniquePtr.h>

namespace ngine::Widgets
{
	struct Widget;
}

namespace ngine::GameFramework
{
	struct Health;
	struct HealthModule;

	struct HealthModule final : public SceneRulesModule
	{
		struct DataSource final : public ngine::DataSource::Interface
		{
			DataSource(SceneRules& sceneRules, HealthModule& healthModule);
			~DataSource();
			// TODO: Update this in assets!
			inline static constexpr Guid DataSourceGuid = "7909FE50-DBFD-43C7-8FBA-CD3F3FCC2653"_guid;
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
			ngine::DataSource::PropertyIdentifier m_healthPropertyIdentifier;
			ngine::DataSource::PropertyIdentifier m_healthRatioPropertyIdentifier;
			SceneRules& m_sceneRules;
			HealthModule& m_healthModule;
		};

		//! Data source stored once per player that has one element per health
		struct PerPlayerDataSource : public DataSource::Interface
		{
			PerPlayerDataSource(Widgets::Widget&, SceneRules& sceneRules, HealthModule& healthModule, const ClientIdentifier clientIdentifier);
			~PerPlayerDataSource();

			// TODO: Ideally we don't reference these guids and instead return something generic like display none to the data source widget.
			// But currenlty the data source doesn't support that.
			inline static constexpr Asset::Guid FilledHealthIcon = "f0882305-09a3-3e30-2519-f7f268ac9acf"_guid;
			inline static constexpr Asset::Guid EmptyHealthIcon = "491a901c-cc36-a2f8-50bc-9bb1a420d7fa"_guid;
			inline static constexpr Guid DataSourceGuid = "1792937f-8551-4828-9a1b-4f70e6eb2ec7"_guid;

			// DataSource::Interface
			virtual GenericDataIndex GetDataCount() const override;
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
			virtual void CacheQuery(const Query& query, CachedQuery& cachedQueryOut) const override;
			// ~DataSource::Interface

			using Interface::OnDataChanged;
		private:
			ClientIdentifier m_clientIdentifier;
			ngine::DataSource::Identifier m_healthPropertyIdentifier;
			SceneRules& m_sceneRules;
			HealthModule& m_healthModule;
		};

		using BaseType = SceneRulesModule;

		inline static constexpr Guid ShowHealthUIEvent = "ae4a7859-02e8-4cd7-8c93-2342b37b849d"_guid;

		struct Initializer : public SceneRulesModule::Initializer
		{
			using BaseType = SceneRulesModule::Initializer;
			using BaseType::BaseType;

			Initializer(BaseType&& initializer)
				: BaseType(Forward<SceneRulesModule::Initializer>(initializer))
			{
			}
		};

		HealthModule(const HealthModule& templateComponent, const Cloner& cloner);
		HealthModule(const Deserializer& deserializer);
		HealthModule(Initializer&& initializer);
		virtual ~HealthModule();

		virtual void OnGameplayStarted(SceneRules&) override;
		virtual void OnGameplayStopped(SceneRules&) override;

		void AddHealth(SceneRules&, const ClientIdentifier clientIdentifier, const float value);
		[[nodiscard]] float GetHealth(SceneRules&, const ClientIdentifier clientIdentifier);
		[[nodiscard]] float GetMaximumHealth(SceneRules&, const ClientIdentifier clientIdentifier);
		[[nodiscard]] Math::Ratiof GetHealthRatio(SceneRules&, const ClientIdentifier clientIdentifier);
	protected:
		void OnPlayerSpawned(SceneRules&, const ClientIdentifier, Entity::Component3D&);
		void OnPlayerHUDLoaded(SceneRules&, const ClientIdentifier);

		void EnableUI(SceneRules&, const ClientIdentifier);

		void SetHealthInternal(SceneRules& sceneRules, Health& playerHealth, const ClientIdentifier clientIdentifier, const float newHealth);

		friend struct Reflection::ReflectedType<HealthModule>;
		struct HealthChangedData
		{
			ClientIdentifier playerIdentifier;
			float health;
		};
		friend struct Reflection::ReflectedType<HealthChangedData>;
		void ClientOnPlayerHealthChanged(
			Entity::HierarchyComponentBase& parent,
			Network::Session::BoundComponent& boundComponent,
			Network::LocalClient& localClient,
			const HealthChangedData data
		);
	private:
		TIdentifierArray<UniquePtr<PerPlayerDataSource>, ClientIdentifier> m_perPlayerDataSources;
		UniquePtr<DataSource> m_pDataSource;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<GameFramework::HealthModule::HealthChangedData>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::HealthModule::HealthChangedData>(
			"{FF0CA73D-EE2F-4E0A-9B5C-E1E91F29ABEE}"_guid,
			MAKE_UNICODE_LITERAL("Player Score Changed Data"),
			TypeFlags{},
			Tags{},
			Properties{
				Property{
					MAKE_UNICODE_LITERAL("Player Identifier"),
					"playerIdentifier",
					"{20AD176A-7912-4FA1-907A-B3046575FF05}"_guid,
					MAKE_UNICODE_LITERAL("PlayerSpawnedData"),
					PropertyFlags::SentWithNetworkedFunctions,
					&GameFramework::HealthModule::HealthChangedData::playerIdentifier
				},
				Property{
					MAKE_UNICODE_LITERAL("Health"),
					"health",
					"{146CE544-F6EE-4B17-8615-69E8E81B0587}"_guid,
					MAKE_UNICODE_LITERAL("PlayerSpawnedData"),
					PropertyFlags::SentWithNetworkedFunctions,
					&GameFramework::HealthModule::HealthChangedData::health
				}
			}
		);
	};

	template<>
	struct ReflectedType<GameFramework::HealthModule>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::HealthModule>(
			"22dce412-2016-4019-8392-90ff0294a32e"_guid,
			MAKE_UNICODE_LITERAL("Health Game Rules Module"),
			TypeFlags{},
			Tags{},
			Properties{},
			Functions{Function{
				"{92448D99-3317-4483-8F45-6D12B4C03708}"_guid,
				MAKE_UNICODE_LITERAL("On Player Health Changed"),
				&GameFramework::HealthModule::ClientOnPlayerHealthChanged,
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
