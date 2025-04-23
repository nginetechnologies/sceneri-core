
#pragma once

#include "Components/SensorComponent.h"

#include <Engine/Entity/Indicator/IndicatorTypeExtension.h>

#include <Common/Function/Event.h>
#include <Common/Reflection/CoreTypes.h>
#include <Common/Reflection/EnumTypeExtension.h>

namespace ngine::Entity
{
	struct Component3D;
}

namespace ngine::GameFramework::Signal::Transmitters
{
	struct SensorTransmitter final : public SensorComponent
	{
		static constexpr Guid TypeGuid = "c90ede96-46b3-4f18-a0d6-ed0d26773916"_guid;
		using BaseType = SensorComponent;
		using BaseType::BaseType;

		SensorTransmitter(Initializer&& initializer);
		SensorTransmitter(const SensorTransmitter& templateComponent, const Cloner& cloner);
		SensorTransmitter(const Deserializer& deserializer);

		void OnCreated();
	protected:
		void OnComponentDetected(SensorComponent&, Optional<Entity::Component3D*> pComponent);
		void OnComponentLost(SensorComponent&, Optional<Entity::Component3D*> pComponent);
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<GameFramework::Signal::Transmitters::SensorTransmitter>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::Signal::Transmitters::SensorTransmitter>(
			GameFramework::Signal::Transmitters::SensorTransmitter::TypeGuid,
			MAKE_UNICODE_LITERAL("Sensor Transmitter"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{Entity::ComponentTypeExtension{
				Entity::ComponentTypeFlags(),
				"14d70621-c3e5-3260-a758-9e4ae6afffae"_asset,
				"ef85043b-303d-43ac-84da-8b920b61fb2b"_guid,
				"e00f4f1c-418d-4a3f-b1f1-fd051d55f8cb"_asset
			}}
		);
	};
}
