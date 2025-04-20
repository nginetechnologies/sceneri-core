#pragma once

#include <Widgets/Data/Component.h>

#include <Common/Time/Stopwatch.h>

namespace ngine::Widgets::Data
{
	struct OpacityOscillator : public Component
	{
		using InstanceIdentifier = TIdentifier<uint32, 12>;
		using BaseType = Component;

		OpacityOscillator(const Deserializer& deserializer);
		OpacityOscillator(const OpacityOscillator& templateComponent, const Cloner& cloner);
		~OpacityOscillator();

		void OnEnable();
		void OnDisable();

		void Update();
	protected:
		friend struct Reflection::ReflectedType<OpacityOscillator>;
		Widget& m_owner;
		float m_speed;
		float m_minimum;
		float m_maximum;
		float m_randomOffset;
		Time::Stopwatch m_stopwatch;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Widgets::Data::OpacityOscillator>
	{
		inline static constexpr auto Type = Reflection::Reflect<Widgets::Data::OpacityOscillator>(
			"365dc70c-ff2b-484a-8d72-a5bad477804f"_guid,
			MAKE_UNICODE_LITERAL("Widget Opacity Oscillator"),
			TypeFlags::DisableDynamicInstantiation,
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Speed"),
					"speed",
					"{2EBA1A7B-BD1E-48D7-8B9D-39BAF84FCFD6}"_guid,
					MAKE_UNICODE_LITERAL("Speed"),
					&Widgets::Data::OpacityOscillator::m_speed
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Minimum"),
					"minimum",
					"{995E9727-7450-4453-AFEE-6272FFF0662E}"_guid,
					MAKE_UNICODE_LITERAL("Minimum Opacity"),
					&Widgets::Data::OpacityOscillator::m_speed
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Maximum"),
					"maximum",
					"{FA54FDF5-B437-4A30-9236-76AAF2671A8C}"_guid,
					MAKE_UNICODE_LITERAL("Maximum Opacity"),
					&Widgets::Data::OpacityOscillator::m_speed
				)
			}
		);
	};
}
