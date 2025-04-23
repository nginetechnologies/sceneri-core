#pragma once

#include "Components/SensorComponent.h"

#include <Engine/Entity/Scene/ComponentTemplateIdentifier.h>

#include <Engine/Entity/Indicator/IndicatorTypeExtension.h>

#include <Common/Asset/Picker.h>

namespace ngine::Entity
{
	struct Component3D;
}

namespace ngine::GameFramework
{
	struct ImpulseSensor final : public SensorComponent
	{
		static constexpr Guid TypeGuid = "da54462b-1339-4cb3-bb4c-e0f5b68e6005"_guid;
		using InstanceIdentifier = TIdentifier<uint32, 8>;
		using BaseType = SensorComponent;
		using BaseType::BaseType;

		ImpulseSensor(Initializer&& initializer);
		ImpulseSensor(const ImpulseSensor& templateComponent, const Cloner& cloner);

		void OnCreated();
	protected:
		void OnComponentDetected(SensorComponent&, Optional<Entity::Component3D*> pComponent);

		using ScenePicker = Asset::Picker;
		void SetSceneAsset(const ScenePicker asset);
		ScenePicker GetSceneAsset() const;

		void SpawnMesh();
	protected:
		friend struct Reflection::ReflectedType<GameFramework::ImpulseSensor>;

		Math::Vector3f m_impulse{0, 0, 500};
		Entity::ComponentTemplateIdentifier m_sceneTemplateIdentifier;
		Optional<Entity::Component3D*> m_pMeshComponent;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<GameFramework::ImpulseSensor>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::ImpulseSensor>(
			GameFramework::ImpulseSensor::TypeGuid,
			MAKE_UNICODE_LITERAL("Impulse Sensor"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Impulse"),
					"impulse",
					"{C64C549F-18C1-4134-9E53-976BD11914F1}"_guid,
					MAKE_UNICODE_LITERAL("Impulse Sensor"),
					Reflection::PropertyFlags::VisibleToParentScope,
					&GameFramework::ImpulseSensor::m_impulse
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Mesh"),
					"mesh",
					"{546D7709-2B74-4BD3-9011-87BA79B7962F}"_guid,
					MAKE_UNICODE_LITERAL("Impulse Sensor"),
					Reflection::PropertyFlags::VisibleToParentScope,
					&GameFramework::ImpulseSensor::SetSceneAsset,
					&GameFramework::ImpulseSensor::GetSceneAsset
				)
			},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{Entity::ComponentTypeExtension{
				Entity::ComponentTypeFlags(),
				"d464f2b3-87bb-7433-dcd3-789b045402b6"_asset,
				"ef85043b-303d-43ac-84da-8b920b61fb2b"_guid,
				"e6abe028-09c1-4f66-991f-999c52bb1358"_asset
			}}
		);
	};
}
