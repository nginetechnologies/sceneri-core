#pragma once

#include <Engine/Entity/Data/Component.h>
#include <Common/Function/Event.h>
#include <Common/Time/Duration.h>

namespace ngine::GameFramework
{
	struct Countdown final : public Entity::Data::Component
	{
		using BaseType = Entity::Data::Component;
		using BaseType::BaseType;
		using ValueType = Time::Durationf;

		Event<void(void*), 24> OnChanged;
		Event<void(void*), 24> OnComplete;
		using Initializer = BaseType::DynamicInitializer;

		Countdown(const Deserializer& deserializer);
		Countdown(const Countdown& templateComponent, const Cloner& cloner);
		Countdown(Initializer&& initializer);

		void operator+=(const ValueType duration)
		{
			Add(duration);
		}

		void operator-=(const ValueType duration)
		{
			Remove(duration);
		}

		void Add(const ValueType duration)
		{
			ApplyDuration(m_duration + duration);
			OnChanged();
		}

		void Remove(const ValueType duration)
		{
			ApplyDuration(m_duration - duration);
			OnChanged();
		}

		[[nodiscard]] ValueType Get() const
		{
			return m_duration;
		}

		[[nodiscard]] operator ValueType() const
		{
			return m_duration;
		}
	protected:
		void ApplyDuration(const ValueType duration);
	protected:
		friend struct Reflection::ReflectedType<GameFramework::Countdown>;
		ValueType m_duration;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<GameFramework::Countdown>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::Countdown>(
			"194FFAFF-AB13-464A-AE69-788646258B9F"_guid,
			MAKE_UNICODE_LITERAL("Countdown Component"),
			Reflection::TypeFlags::DisableWriteToDisk,
			Reflection::Tags{},
			Reflection::Properties{Reflection::MakeProperty(
				MAKE_UNICODE_LITERAL("Duration"),
				"duration",
				"{2F1C6CE2-A551-4513-B532-E4914186DBBE}"_guid,
				MAKE_UNICODE_LITERAL("Duration"),
				&GameFramework::Countdown::m_duration
			)}
		);
	};
}
