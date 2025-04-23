#pragma once

#include <GameFramework/Components/SceneRules/Modules/Module.h>

#include <Engine/DataSource/PropertySourceInterface.h>

#include <Common/Time/Stopwatch.h>
#include <Common/Memory/Containers/InlineVector.h>

#include <Common/Memory/UniquePtr.h>

namespace ngine::Widgets
{
	struct Widget;
}

namespace ngine::GameFramework
{
	struct TimerModule;

	struct TimerDataSource : public PropertySource::Interface
	{
		TimerDataSource(Widgets::Widget&, TimerModule&);
		~TimerDataSource();
		inline static constexpr Guid DataSourceGuid = "9877f5a3-b129-47a6-ac83-12704941f0e4"_guid;
		// PropertySource::Interface
		virtual PropertyValue GetDataProperty(const DataSource::PropertyIdentifier identifier) const override;
		// ~PropertySource::Interface

		void OnChanged();
	private:
		DataSource::PropertyIdentifier m_timePropertyIdentifier;
		TimerModule& m_timer;
	};

	struct TimerModule final : public SceneRulesModule
	{
		using BaseType = SceneRulesModule;

		TimerModule(const TimerModule& templateComponent, const Cloner& cloner);
		TimerModule(const Deserializer& deserializer);
		TimerModule(Initializer&& initializer);
		virtual ~TimerModule();

		void OnCreated();
		void Update();

		virtual void OnGameplayStarted(SceneRules&) override;
		virtual void OnGameplayPaused(SceneRules&) override;
		virtual void OnGameplayResumed(SceneRules&) override;
		virtual void OnGameplayStopped(SceneRules&) override;

		void OnPlayerHUDLoaded(SceneRules&, const ClientIdentifier);

		[[nodiscard]] Time::Durationf GetElapsedTime() const
		{
			return m_timer.GetElapsedTime();
		}
	protected:
		void RegisterForUpdate(Entity::Component3D& owner);
		void DeregisterUpdate(Entity::Component3D& owner);
	private:
		Time::Stopwatch m_timer;
		UniquePtr<TimerDataSource> m_pTimerDataSource;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<GameFramework::TimerModule>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::TimerModule>(
			"1c0d27be-2afa-4a57-9d02-3e797c44af27"_guid, MAKE_UNICODE_LITERAL("Timer Game Rules Module")
		);
	};
}
