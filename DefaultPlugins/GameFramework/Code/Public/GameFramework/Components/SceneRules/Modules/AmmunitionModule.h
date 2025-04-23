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
	struct Ammunition;
	struct AmmunitionModule;

	struct AmmunitionDataSource : public PropertySource::Interface
	{
		AmmunitionDataSource(Widgets::Widget&, AmmunitionModule&);
		~AmmunitionDataSource();
		inline static constexpr Guid DataSourceGuid = "45db6d17-4498-46b8-bdf8-8482f612e60a"_guid;
		// PropertySource::Interface
		virtual PropertyValue GetDataProperty(const DataSource::PropertyIdentifier identifier) const override;
		// ~PropertySource::Interface

		void OnChanged();
	private:
		DataSource::PropertyIdentifier m_ammunitionPropertyIdentifier;
		DataSource::PropertyIdentifier m_maxAmmunitionPropertyIdentifier;
		AmmunitionModule& m_ammunition;
	};

	struct AmmunitionModule final : public SceneRulesModule
	{
		inline static constexpr Guid TypeGuid = "8963059c-1a83-4475-88f3-f094f1a2c3e2"_guid;

		using BaseType = SceneRulesModule;

		inline static constexpr Guid ShowAmmunitionUIEvent = "ba2a72ab-8bbb-4f09-85e9-6fb4d2dfcfcb"_guid;
		inline static constexpr Guid AmmunitionChangedUIEvent = "569d6512-be8b-4570-b3e2-edcbe07543a1"_guid;

		struct Initializer : public SceneRulesModule::Initializer
		{
			using BaseType = SceneRulesModule::Initializer;
			using BaseType::BaseType;

			Initializer(BaseType&& initializer)
				: BaseType(Forward<SceneRulesModule::Initializer>(initializer))
			{
			}
		};

		AmmunitionModule(const AmmunitionModule& templateComponent, const Cloner& cloner);
		AmmunitionModule(const Deserializer& deserializer);
		AmmunitionModule(Initializer&& initializer);
		virtual ~AmmunitionModule();

		void OnCreated();

		virtual void OnGameplayStarted(SceneRules&) override;
		virtual void OnGameplayStopped(SceneRules&) override;

		void OnAmmunitionChanged(SceneRules&, const ClientIdentifier);
		void OnMaxAmmunitionChanged(SceneRules&, const ClientIdentifier);

		void OnPlayerSpawned(SceneRules&, const ClientIdentifier, Entity::Component3D&);
		void OnPlayerHUDLoaded(SceneRules&, const ClientIdentifier);

		void EnableUI(SceneRules&, const ClientIdentifier);
	private:
		friend AmmunitionDataSource;

		Ammunition* m_pAmmunition = nullptr;

		UniquePtr<AmmunitionDataSource> m_pAmmunitionDataSource;
	};

}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<GameFramework::AmmunitionModule>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::AmmunitionModule>(
			GameFramework::AmmunitionModule::TypeGuid, MAKE_UNICODE_LITERAL("Ammunition Game Rules Module")
		);
	};
}
