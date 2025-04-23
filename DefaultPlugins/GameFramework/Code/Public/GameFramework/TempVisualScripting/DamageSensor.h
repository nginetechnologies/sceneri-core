#pragma once

#include "Components/SensorComponent.h"
#include "Components/Player/Health.h"

#include <Engine/Entity/Scene/ComponentTemplateIdentifier.h>

#include <Engine/Entity/Indicator/IndicatorTypeExtension.h>

#include <Common/Math/ClampedValue.h>
#include <Common/Asset/Picker.h>
#include <Common/Threading/Jobs/TimerHandle.h>

namespace ngine::Entity
{
	struct Component3D;
}

namespace ngine::Audio
{
	struct SoundSpotComponent;
}

namespace ngine::Network
{
	struct LocalClient;

	namespace Session
	{
		struct BoundComponent;
	}
}

namespace ngine::GameFramework
{
	struct DamageSensorComponent final : public SensorComponent
	{
		static constexpr Guid TypeGuid = "7d07e1f4-1660-4732-9ab0-ca3c4bad63ff"_guid;

		using InstanceIdentifier = TIdentifier<uint32, 9>;

		using BaseType = SensorComponent;
		using BaseType::BaseType;

		DamageSensorComponent(Initializer&& initializer);
		DamageSensorComponent(const DamageSensorComponent& templateComponent, const Cloner& cloner);

		using AudioAssetPicker = Asset::Picker;
		void SetAudioAsset(const AudioAssetPicker asset);
		AudioAssetPicker GetAudioAsset() const;

		void OnCreated();
	protected:
		void Collect(SensorComponent&, Optional<Entity::Component3D*> pComponent);

		using ScenePicker = Asset::Picker;
		void SetSceneAsset(const ScenePicker asset);
		ScenePicker GetSceneAsset() const;

		void SpawnMesh();

		void ClientOnTriggeredOnHost(Network::Session::BoundComponent& boundComponent, Network::LocalClient& localClient);
		void OnTriggeredInternal();

		[[nodiscard]] Entity::Component3D& GetOwner();
	protected:
		friend struct Reflection::ReflectedType<DamageSensorComponent>;

		Math::ClampedValue<Health::ValueType> m_damage = {1, 0, Math::NumericLimits<Health::ValueType>::Max};
		Optional<Audio::SoundSpotComponent*> m_pSoundSpotComponent;
		Asset::Guid m_audioAssetGuid;
		Entity::ComponentTemplateIdentifier m_sceneTemplateIdentifier;
		Optional<Entity::Component3D*> m_pMeshComponent;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<GameFramework::DamageSensorComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::DamageSensorComponent>(
			GameFramework::DamageSensorComponent::TypeGuid,
			MAKE_UNICODE_LITERAL("Damage Sensor"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Damage"),
					"damage",
					"{692AAB32-3A83-4324-B171-7DF98B3C5D58}"_guid,
					MAKE_UNICODE_LITERAL("Damage Sensor"),
					Reflection::PropertyFlags::VisibleToParentScope,
					&GameFramework::DamageSensorComponent::m_damage
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Collect Sound"),
					"audio_asset",
					"{A785A857-5767-4858-9764-775A64BCC024}"_guid,
					MAKE_UNICODE_LITERAL("Damage Sensor"),
					Reflection::PropertyFlags::VisibleToParentScope,
					&GameFramework::DamageSensorComponent::SetAudioAsset,
					&GameFramework::DamageSensorComponent::GetAudioAsset
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Mesh"),
					"mesh",
					"{E937CD59-D10B-4C0E-980C-86F7C0044D3F}"_guid,
					MAKE_UNICODE_LITERAL("Damage Sensor"),
					Reflection::PropertyFlags::VisibleToParentScope,
					&GameFramework::DamageSensorComponent::SetSceneAsset,
					&GameFramework::DamageSensorComponent::GetSceneAsset
				)
			},
			Reflection::Functions{Function{
				"{84A6A97E-DB7D-46AF-A7BF-C963CECBBC06}"_guid,
				MAKE_UNICODE_LITERAL("On Triggered On Host"),
				&GameFramework::DamageSensorComponent::ClientOnTriggeredOnHost,
				FunctionFlags::HostToClient,
				Reflection::ReturnType{},
				Reflection::Argument{"7b013251-7b9f-4a06-8333-cac15431117d"_guid, MAKE_UNICODE_LITERAL("boundComponent")},
				Reflection::Argument{"ec306a54-3f68-49af-94f3-bffe806b20f3"_guid, MAKE_UNICODE_LITERAL("localClient")}
			}},
			Reflection::Events{},
			Reflection::Extensions{Entity::ComponentTypeExtension{
				Entity::ComponentTypeFlags(),
				"eebed34e-7cff-7a0b-742a-f92bef66a445"_asset,
				"ef85043b-303d-43ac-84da-8b920b61fb2b"_guid,
				"72043a52-4674-407b-aa47-865f43cb077b"_asset
			}}
		);
	};
}
