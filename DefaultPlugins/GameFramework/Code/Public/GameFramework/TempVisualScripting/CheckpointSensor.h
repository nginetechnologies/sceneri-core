
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

	struct CheckpointSensor final : public SensorComponent
	{
		static constexpr Guid TypeGuid = "767aa5d4-9881-4368-8a59-07b6942f2089"_guid;
		using InstanceIdentifier = TIdentifier<uint32, 8>;
		using BaseType = SensorComponent;
		using BaseType::BaseType;

		CheckpointSensor(Initializer&& initializer);
		CheckpointSensor(const CheckpointSensor& templateComponent, const Cloner& cloner);

		void OnCreated();
	protected:
		void OnEnter(SensorComponent&, Optional<Entity::Component3D*> pComponent);

		using ScenePicker = Asset::Picker;
		void SetSceneAsset(const ScenePicker asset);
		ScenePicker GetSceneAsset() const;

		void SpawnMesh();
	protected:
		friend struct Reflection::ReflectedType<CheckpointSensor>;

		void SetSpawnPoint(const Entity::Component3DPicker spawnPoint);
		Entity::Component3DPicker GetSpawnPoint() const;

		Entity::ComponentSoftReference m_spawnPoint;
		Entity::ComponentTemplateIdentifier m_sceneTemplateIdentifier;
		Optional<Entity::Component3D*> m_pMeshComponent;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<GameFramework::CheckpointSensor>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::CheckpointSensor>(
			GameFramework::CheckpointSensor::TypeGuid,
			MAKE_UNICODE_LITERAL("Checkpoint Sensor"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Spawn Point"),
					"spawnPoint",
					"{90640833-A870-435C-8A53-E0BF01684C96}"_guid,
					MAKE_UNICODE_LITERAL("Checkpoint Sensor"),
					Reflection::PropertyFlags::VisibleToParentScope,
					&GameFramework::CheckpointSensor::SetSpawnPoint,
					&GameFramework::CheckpointSensor::GetSpawnPoint
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Mesh"),
					"mesh",
					"{2F6A8380-8214-4F65-A6D5-C40C76D20C86}"_guid,
					MAKE_UNICODE_LITERAL("Checkpoint Sensor"),
					Reflection::PropertyFlags::VisibleToParentScope,
					&GameFramework::CheckpointSensor::SetSceneAsset,
					&GameFramework::CheckpointSensor::GetSceneAsset
				)
			},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{Entity::ComponentTypeExtension{
				Entity::ComponentTypeFlags(),
				"395010bf-aff6-c46a-cc54-9853e99c611e"_asset,
				"ef85043b-303d-43ac-84da-8b920b61fb2b"_guid,
				"d4996837-d3ae-4a3e-af3c-99557059ac39"_asset
			}}
		);
	};
}
