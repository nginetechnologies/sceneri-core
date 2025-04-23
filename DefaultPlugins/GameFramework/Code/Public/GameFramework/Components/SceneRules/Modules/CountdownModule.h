#pragma once

#include <GameFramework/Components/SceneRules/Modules/Module.h>
#include <Common/Time/Stopwatch.h>
#include <Common/EnumFlags.h>

#include <Engine/DataSource/PropertySourceInterface.h>

#include <Common/Memory/UniquePtr.h>
#include <Common/Function/Event.h>

namespace ngine::Widgets
{
	struct Widget;
}

namespace ngine::GameFramework
{
	struct CountdownModule;

	struct CountdownDataSource : public PropertySource::Interface
	{
		CountdownDataSource(Widgets::Widget&, CountdownModule&);
		~CountdownDataSource();
		inline static constexpr Guid DataSourceGuid = "24265b02-bd16-437d-bea1-394f7884802d"_guid;
		// PropertySource::Interface
		virtual PropertyValue GetDataProperty(const DataSource::PropertyIdentifier identifier) const override;
		// ~PropertySource::Interface

		void OnChanged();
	private:
		DataSource::PropertyIdentifier m_countdownPropertyIdentifier;
		CountdownModule& m_countdown;
	};

	struct CountdownModule final : public SceneRulesModule
	{
		using BaseType = SceneRulesModule;

		inline static constexpr Guid CountdownEndEvent = "0c25e3a7-f5ba-4c55-acbc-2b847efdd46e"_guid;
		inline static constexpr Guid CountdownStartEvent = "f9b5eb8a-cbe0-4595-8546-dc5d08e061cc"_guid;
		inline static constexpr Guid ShowCountdownUIEvent = "83794BA2-0790-429C-85F1-4B18FDB18A83"_guid;

		enum class Options : uint8
		{
			SleepPlayerPhysics = 1 << 0,
			// BlockPlayerInput = 1 << 1,
		};

		struct Initializer : public SceneRulesModule::Initializer
		{
			using BaseType = SceneRulesModule::Initializer;
			using BaseType::BaseType;

			Initializer(BaseType&& initializer, EnumFlags<Options> options = {})
				: BaseType(Forward<SceneRulesModule::Initializer>(initializer))
				, m_options(options)
			{
			}

			EnumFlags<Options> m_options;
		};

		CountdownModule(const CountdownModule& templateComponent, const Cloner& cloner);
		CountdownModule(const Deserializer& deserializer);
		CountdownModule(Initializer&& initializer);
		virtual ~CountdownModule();

		void OnCreated();
		void Update();

		virtual void OnGameplayStarted(SceneRules&) override;
		virtual void OnGameplayPaused(SceneRules&) override;
		virtual void OnGameplayResumed(SceneRules&) override;
		virtual void OnGameplayStopped(SceneRules&) override;

		void OnPlayerSpawned(SceneRules&, const ClientIdentifier, Entity::Component3D&);
		void OnPlayerHUDLoaded(SceneRules&, const ClientIdentifier);

		Event<void(void*)> OnCountdownStarted;
		Event<void(void*)> OnCountdownEnded;

		void EnableUI(SceneRules&, const ClientIdentifier);

		Time::Durationf GetRemainingTime() const;
	protected:
		void RegisterForUpdate(Entity::Component3D& owner);
		void DeregisterUpdate(Entity::Component3D& owner);
	private:
		Optional<SceneRules*> m_pSceneRules;

		Time::Stopwatch m_stopwatch;
		Time::Durationf m_duration{0_seconds};
		EnumFlags<Options> m_options{};
		UniquePtr<CountdownDataSource> m_pCountdownDataSource;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<GameFramework::CountdownModule>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::CountdownModule>(
			"7adde0df-a07d-45dc-999e-80066aed55a9"_guid, MAKE_UNICODE_LITERAL("Countdown Game Rules Module")
		);
	};
}
