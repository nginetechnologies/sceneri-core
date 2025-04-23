#pragma once

#include <GameFramework/Components/SceneRules/Modules/Module.h>

#include <Engine/DataSource/PropertySourceInterface.h>

#include <Common/Function/Event.h>
#include <Common/Time/Stopwatch.h>
#include <Common/EnumFlags.h>

namespace ngine::GameFramework
{
	struct StartCountdownModule final : public SceneRulesModule, public PropertySource::Interface
	{
		using BaseType = SceneRulesModule;

		inline static constexpr Guid DataSourceGuid = "c98b70aa-12f5-46b5-80ca-327f2ac078df"_guid;

		inline static constexpr Guid CountdownEndEvent = "0c25e3a7-f5ba-4c55-acbc-2b847efdd46e"_guid;
		inline static constexpr Guid CountdownStartEvent = "f9b5eb8a-cbe0-4595-8546-dc5d08e061cc"_guid;

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

		StartCountdownModule(const StartCountdownModule& templateComponent, const Cloner& cloner);
		StartCountdownModule(const Deserializer& deserializer);
		StartCountdownModule(Initializer&& initializer);
		virtual ~StartCountdownModule();

		void OnCreated();
		void Update();

		Event<void(void*)> OnCountdownStarted;
		Event<void(void*)> OnCountdownEnded;

		virtual void OnGameplayStarted(SceneRules&) override;
		virtual void OnGameplayStopped(SceneRules&) override;

		void OnPlayerSpawned(SceneRules&, const ClientIdentifier, Entity::Component3D&);
		void OnPlayerHUDLoaded(SceneRules&, const ClientIdentifier);

		void EndCountdown(SceneRules&);

		[[nodiscard]] Time::Durationf GetTimeLeft() const
		{
			return m_countdownTime - m_timer.GetElapsedTime();
		}

		// PropertySource::Interface
		virtual PropertyValue GetDataProperty(const DataSource::PropertyIdentifier identifier) const override;
		// ~PropertySource::Interface
	protected:
		void RegisterForUpdate(Entity::Component3D& owner);
		void DeregisterUpdate(Entity::Component3D& owner);
	private:
		Time::Durationf m_countdownTime = 3_seconds;
		Time::Stopwatch m_timer;
		EnumFlags<Options> m_options{};

		DataSource::PropertyIdentifier m_countdownPropertyIdentifier;
	};

	ENUM_FLAG_OPERATORS(StartCountdownModule::Options);
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<GameFramework::StartCountdownModule>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::StartCountdownModule>(
			"74ca5ead-6655-4609-8dfd-98423def13d5"_guid, MAKE_UNICODE_LITERAL("Start Countdown Game Rules Module")
		);
	};
}
