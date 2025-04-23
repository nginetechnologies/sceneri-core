#pragma once

#include <GameFramework/Components/SceneRules/Modules/Module.h>

#include <Engine/DataSource/PropertySourceInterface.h>

#include <Common/Memory/UniquePtr.h>
#include <Common/Function/Event.h>

namespace ngine::Widgets
{
	struct Widget;
}

namespace ngine::GameFramework
{
	struct KDCounter;
	struct KDCounterModule;

	struct KDCounterDataSource : public PropertySource::Interface
	{
		KDCounterDataSource(Widgets::Widget&, KDCounterModule&);
		~KDCounterDataSource();
		inline static constexpr Guid DataSourceGuid = "e9a0e0b9-e1d4-45e7-bffa-b015c065138a"_guid;
		// PropertySource::Interface
		virtual PropertyValue GetDataProperty(const DataSource::PropertyIdentifier identifier) const override;
		// ~PropertySource::Interface

		void OnChanged();
	private:
		DataSource::PropertyIdentifier m_killsPropertyIdentifier;
		DataSource::PropertyIdentifier m_assistsPropertyIdentifier;
		DataSource::PropertyIdentifier m_deathsPropertyIdentifier;
		KDCounterModule& m_counter;
	};

	struct KDCounterModule final : public SceneRulesModule
	{
		inline static constexpr Guid TypeGuid = "c7fc013b-c806-486b-a3a3-30285721d9b9"_guid;
		using BaseType = SceneRulesModule;

		inline static constexpr Guid ShowKDCounterUIEvent = "93c4192c-bb2c-4b41-bddf-bee08f02c78d"_guid;
		inline static constexpr Guid KDCounterChangedUIEvent = "bbf00f17-5381-4c98-a071-2c11a1b86a49"_guid;

		struct Initializer : public SceneRulesModule::Initializer
		{
			using BaseType = SceneRulesModule::Initializer;
			using BaseType::BaseType;

			Initializer(BaseType&& initializer)
				: BaseType(Forward<SceneRulesModule::Initializer>(initializer))
			{
			}
		};

		KDCounterModule(const KDCounterModule& templateComponent, const Cloner& cloner);
		KDCounterModule(const Deserializer& deserializer);
		KDCounterModule(Initializer&& initializer);
		virtual ~KDCounterModule();

		void OnCreated();

		virtual void OnGameplayStarted(SceneRules&) override;
		virtual void OnGameplayStopped(SceneRules&) override;

		void OnKDCounterChanged(SceneRules&, const ClientIdentifier);
		void OnPlayerSpawned(SceneRules&, const ClientIdentifier, Entity::Component3D&);
		void OnPlayerHUDLoaded(SceneRules&, const ClientIdentifier);

		void EnableUI(SceneRules&, const ClientIdentifier);

		[[nodiscard]] Optional<KDCounter*> GetKDCounter() const
		{
			return m_pKDCounter;
		}
	private:
		friend KDCounterDataSource;

		Optional<KDCounter*> m_pKDCounter;
		UniquePtr<KDCounterDataSource> m_pKDCounterDataSource;
	};

}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<GameFramework::KDCounterModule>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::KDCounterModule>(
			GameFramework::KDCounterModule::TypeGuid, MAKE_UNICODE_LITERAL("KDCounter Game Rules Module")
		);
	};
}
