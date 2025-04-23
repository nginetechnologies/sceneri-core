#pragma once

#include "Components/SensorComponent.h"

#include <Engine/Entity/Indicator/IndicatorTypeExtension.h>

#include <Common/Function/Event.h>
#include <Common/Math/Mass.h>

namespace ngine::Entity
{
	struct Component3D;
}

namespace ngine::GameFramework::Signal::Transmitters
{
	struct PressurePlate final : public SensorComponent
	{
		static constexpr Guid TypeGuid = "f65e6195-b9f8-4422-90d9-fea73cbb80c7"_guid;
		enum class State : uint8
		{
			Active,
			Inactive
		};

		using BaseType = SensorComponent;
		using BaseType::BaseType;

		PressurePlate(Initializer&& initializer);
		PressurePlate(const PressurePlate& templateComponent, const Cloner& cloner);
		PressurePlate(const Deserializer& deserializer);

		void OnCreated();
	private:
		friend struct Reflection::ReflectedType<PressurePlate>;

		void CreateReset();
		void OnComponentDetected(SensorComponent&, Optional<Entity::Component3D*> pComponent);
		void OnComponentLost(SensorComponent&, Optional<Entity::Component3D*> pComponent);

		Math::Massf m_massThreshold = 0_grams;
		Math::Massf m_currentMass = 0_grams;
		State m_state = State::Inactive;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<GameFramework::Signal::Transmitters::PressurePlate>
	{
		static constexpr auto Type = Reflection::Reflect<GameFramework::Signal::Transmitters::PressurePlate>(
			GameFramework::Signal::Transmitters::PressurePlate::TypeGuid,
			MAKE_UNICODE_LITERAL("Pressure Plate Transmitter"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{Reflection::MakeProperty(
				MAKE_UNICODE_LITERAL("Mass Threshold"),
				"mass_threshold",
				"{90FF3199-7562-4353-8378-9C3807169B98}"_guid,
				MAKE_UNICODE_LITERAL("Pressure Plate"),
				Reflection::PropertyFlags::VisibleToParentScope,
				&GameFramework::Signal::Transmitters::PressurePlate::m_massThreshold
			)},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{Entity::ComponentTypeExtension{
				Entity::ComponentTypeFlags(),
				"14d70621-c3e5-3260-a758-9e4ae6afffae"_asset,
				"ef85043b-303d-43ac-84da-8b920b61fb2b"_guid,
				"1cb6a239-dba3-42f6-82d2-02a5aa0c0966"_asset
			}}
		);
	};
}
