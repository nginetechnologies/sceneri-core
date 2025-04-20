#pragma once

#include <Widgets/Data/Component.h>

#include <Common/Math/RotationalSpeed.h>
#include <Common/Time/Stopwatch.h>

namespace ngine::Widgets::Data
{
	struct ConstantRotation : public Component
	{
		using InstanceIdentifier = TIdentifier<uint32, 12>;
		using BaseType = Component;

		ConstantRotation(const Deserializer& deserializer);
		ConstantRotation(const ConstantRotation& templateComponent, const Cloner& cloner);
		~ConstantRotation();

		void OnEnable();
		void OnDisable();

		void Update();
	protected:
		friend struct Reflection::ReflectedType<ConstantRotation>;
		Widget& m_owner;
		Math::RotationalSpeedf m_speed;
		Time::Stopwatch m_stopwatch;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Widgets::Data::ConstantRotation>
	{
		inline static constexpr auto Type = Reflection::Reflect<Widgets::Data::ConstantRotation>(
			"ED32542E-8C36-455E-B67C-0D0D34D2E68D"_guid,
			MAKE_UNICODE_LITERAL("Widget Constant Rotation"),
			TypeFlags::DisableDynamicInstantiation,
			Reflection::Tags{},
			Reflection::Properties{Reflection::MakeProperty(
				MAKE_UNICODE_LITERAL("Speed"),
				"speed",
				"{EA5CE7A2-C71A-44A4-A4FC-2C6FE2DE0EB7}"_guid,
				MAKE_UNICODE_LITERAL("Constant Rotation"),
				&Widgets::Data::ConstantRotation::m_speed
			)}
		);
	};
}
