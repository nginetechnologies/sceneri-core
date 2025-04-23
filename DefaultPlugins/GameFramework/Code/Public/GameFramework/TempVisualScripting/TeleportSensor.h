#pragma once

#include "Components/SensorComponent.h"

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
	struct TeleportSensor final : public SensorComponent
	{
		static constexpr Guid TypeGuid = "93a30ea5-1b2f-4261-a73c-d0302e0bed33"_guid;
		using BaseType = SensorComponent;
		using BaseType::BaseType;

		TeleportSensor(const TeleportSensor& templateComponent, const Cloner& cloner);
		TeleportSensor(Initializer&& initializer);

		void OnCreated();
	protected:
		friend struct Reflection::ReflectedType<GameFramework::TeleportSensor>;

		void Teleport(SensorComponent&, Optional<Entity::Component3D*> pComponent);

		void SetTarget(const Entity::Component3DPicker receiver);
		Entity::Component3DPicker GetTarget() const;

		using ScenePicker = Asset::Picker;
		void SetSceneAsset(const ScenePicker asset);
		ScenePicker GetSceneAsset() const;

		void SpawnMesh();
	protected:
		Entity::ComponentSoftReference m_target;
		Entity::ComponentTemplateIdentifier m_sceneTemplateIdentifier;
		Optional<Entity::Component3D*> m_pMeshComponent;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<GameFramework::TeleportSensor>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::TeleportSensor>(
			GameFramework::TeleportSensor::TypeGuid,
			MAKE_UNICODE_LITERAL("Teleport Sensor"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Target"),
					"target",
					"{4ED43F03-90B9-4DBC-8A86-338F66BC6A8D}"_guid,
					MAKE_UNICODE_LITERAL("Teleport Sensor"),
					Reflection::PropertyFlags::VisibleToParentScope,
					&GameFramework::TeleportSensor::SetTarget,
					&GameFramework::TeleportSensor::GetTarget
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Mesh"),
					"mesh",
					"{C1028306-D6ED-4177-8DC5-EC33B66AC947}"_guid,
					MAKE_UNICODE_LITERAL("Teleport Sensor"),
					Reflection::PropertyFlags::VisibleToParentScope,
					&GameFramework::TeleportSensor::SetSceneAsset,
					&GameFramework::TeleportSensor::GetSceneAsset
				)
			},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{Entity::ComponentTypeExtension{
				Entity::ComponentTypeFlags(),
				"9bc4a608-b98e-d482-f5e0-a29318a57880"_asset,
				"ef85043b-303d-43ac-84da-8b920b61fb2b"_guid,
				"3d7b1a60-9c4c-4365-9e0f-6f9e9c89c6f8"_asset
			}}
		);
	};
}
