#pragma once

#include "Components/SensorComponent.h"

#include <Engine/Entity/Indicator/IndicatorTypeExtension.h>

#include <Engine/Entity/ComponentPicker.h>

namespace ngine::Entity
{
	struct Component3D;
}

namespace ngine::GameFramework
{
	struct RespawnSensor final : public SensorComponent
	{
		static constexpr Guid TypeGuid = "38679bb8-9365-438c-83dc-953fa09e6380"_guid;
		using InstanceIdentifier = TIdentifier<uint32, 8>;
		using BaseType = SensorComponent;
		using BaseType::BaseType;

		RespawnSensor(Initializer&& initializer);
		RespawnSensor(const RespawnSensor& templateComponent, const Cloner& cloner);

		void OnCreated();
	protected:
		void Respawn(SensorComponent&, Optional<Entity::Component3D*> pComponent);
	protected:
		friend struct Reflection::ReflectedType<RespawnSensor>;

		void SetCustomSpawnPoint(const Entity::Component3DPicker spawnPoint);
		Entity::Component3DPicker GetCustomSpawnPoint() const;

		Entity::ComponentSoftReference m_customSpawnPoint;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<GameFramework::RespawnSensor>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::RespawnSensor>(
			GameFramework::RespawnSensor::TypeGuid,
			MAKE_UNICODE_LITERAL("Respawn Sensor"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{Reflection::MakeDynamicProperty(
				MAKE_UNICODE_LITERAL("Spawn Point"),
				"spawnPoint",
				"{7F8E1508-6380-4ED6-8CC7-AD2019BB9E8B}"_guid,
				MAKE_UNICODE_LITERAL("Respawn Sensor"),
				Reflection::PropertyFlags::VisibleToParentScope,
				&GameFramework::RespawnSensor::SetCustomSpawnPoint,
				&GameFramework::RespawnSensor::GetCustomSpawnPoint
			)}
		);
	};
}
