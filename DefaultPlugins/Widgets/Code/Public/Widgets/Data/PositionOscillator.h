#pragma once

#include <Widgets/Data/Component.h>

#include <Common/Time/Stopwatch.h>
#include <Common/Math/Vector2.h>

namespace ngine::Widgets::Data
{
	struct PositionOscillator : public Component
	{
		using InstanceIdentifier = TIdentifier<uint32, 12>;
		using BaseType = Component;

		PositionOscillator(const Deserializer& deserializer);
		PositionOscillator(const PositionOscillator& templateComponent, const Cloner& cloner);
		~PositionOscillator();

		void OnDestroying(ParentType& owner);

		void OnEnable(ParentType& owner);
		void OnDisable(ParentType& owner);

		void Update();
	protected:
		friend struct Reflection::ReflectedType<PositionOscillator>;
		Widget& m_owner;
		float m_speed;
		float m_duration;
		Math::Vector2f m_maximum;
		Math::Vector2f m_start;
		Guid m_triggerGuid;
		Time::Stopwatch m_stopwatch;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Widgets::Data::PositionOscillator>
	{
		inline static constexpr auto Type = Reflection::Reflect<Widgets::Data::PositionOscillator>(
			"9d2427cb-47d1-4c5f-a12a-f60e300b7770"_guid,
			MAKE_UNICODE_LITERAL("Widget Position Oscillator"),
			TypeFlags::DisableDynamicInstantiation,
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Speed"),
					"speed",
					"{7C3FA43B-F9C6-4778-A6C6-B3BF51425803}"_guid,
					MAKE_UNICODE_LITERAL("Speed"),
					&Widgets::Data::PositionOscillator::m_speed
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Event Guid"),
					"event",
					"{D23892E6-A936-4249-AF23-7D2E925047EE}"_guid,
					MAKE_UNICODE_LITERAL("Event Guid"),
					&Widgets::Data::PositionOscillator::m_triggerGuid
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Maximum"),
					"maximum",
					"{45D4E947-73F2-44E8-B6B7-06B458A366EE}"_guid,
					MAKE_UNICODE_LITERAL("Maximum Position"),
					&Widgets::Data::PositionOscillator::m_maximum
				)
			}
		);
	};
}
