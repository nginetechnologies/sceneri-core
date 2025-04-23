
#pragma once

#include "Components/SensorComponent.h"
#include <Common/Function/Event.h>

#include <Engine/Entity/Scene/ComponentTemplateIdentifier.h>
#include <Engine/Entity/ComponentPicker.h>
#include <Engine/Entity/Indicator/IndicatorTypeExtension.h>

#include <Common/Asset/Picker.h>

namespace ngine::Entity
{
	struct Component3D;
}

namespace ngine::GameFramework
{
	struct GameRulesBase;

	struct HazardSensor final : public SensorComponent
	{
		static constexpr Guid TypeGuid = "bb15f0a1-1644-44b2-9f6c-4c09ec7c7370"_guid;
		using InstanceIdentifier = TIdentifier<uint32, 8>;
		using BaseType = SensorComponent;
		using BaseType::BaseType;

		HazardSensor(Initializer&& initializer);
		HazardSensor(const HazardSensor& templateComponent, const Cloner& cloner);

		void OnCreated();
	protected:
		void Restart(SensorComponent&, Optional<Entity::Component3D*> pComponent);

		using ScenePicker = Asset::Picker;
		void SetSceneAsset(const ScenePicker asset);
		ScenePicker GetSceneAsset() const;

		void SpawnMesh();
	protected:
		friend struct Reflection::ReflectedType<HazardSensor>;

		void SetCustomSpawnPoint(const Entity::Component3DPicker spawnPoint);
		Entity::Component3DPicker GetCustomSpawnPoint() const;

		Entity::ComponentSoftReference m_customSpawnPoint;
		Entity::ComponentTemplateIdentifier m_sceneTemplateIdentifier;
		Optional<Entity::Component3D*> m_pMeshComponent;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<GameFramework::HazardSensor>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::HazardSensor>(
			GameFramework::HazardSensor::TypeGuid,
			MAKE_UNICODE_LITERAL("Hazard Sensor"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Spawn Point"),
					"spawnPoint",
					"{90640833-A870-435C-8A53-E0BF01684C96}"_guid,
					MAKE_UNICODE_LITERAL("Hazard Sensor"),
					Reflection::PropertyFlags::VisibleToParentScope,
					&GameFramework::HazardSensor::SetCustomSpawnPoint,
					&GameFramework::HazardSensor::GetCustomSpawnPoint
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Mesh"),
					"mesh",
					"{2F6A8380-8214-4F65-A6D5-C40C76D20C86}"_guid,
					MAKE_UNICODE_LITERAL("Hazard Sensor"),
					Reflection::PropertyFlags::VisibleToParentScope,
					&GameFramework::HazardSensor::SetSceneAsset,
					&GameFramework::HazardSensor::GetSceneAsset
				)
			},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{Entity::ComponentTypeExtension{
				Entity::ComponentTypeFlags(),
				"395010bf-aff6-c46a-cc54-9853e99c611e"_asset,
				"ef85043b-303d-43ac-84da-8b920b61fb2b"_guid,
				"6352c2a9-1ae2-42a9-b630-53ee3db19bf3"_asset
			}}
		);
	};
}
