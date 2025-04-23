#pragma once

#include "Components/SensorComponent.h"

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
	struct DestroySensor final : public SensorComponent
	{
		static constexpr Guid TypeGuid = "d0879fbb-891e-494d-bba9-d36fabcad38a"_guid;

		using InstanceIdentifier = TIdentifier<uint32, 9>;

		using BaseType = SensorComponent;
		using BaseType::BaseType;

		DestroySensor(Initializer&& initializer);
		DestroySensor(const DestroySensor& templateComponent, const Cloner& cloner);

		using AudioAssetPicker = Asset::Picker;
		void SetAudioAsset(const AudioAssetPicker asset);
		AudioAssetPicker GetAudioAsset() const;

		void OnCreated();
	protected:
		void OnContact(SensorComponent&, Optional<Entity::Component3D*> pComponent);

		using ScenePicker = Asset::Picker;
		void SetSceneAsset(const ScenePicker asset);
		ScenePicker GetSceneAsset() const;

		void SpawnMesh();

		void ClientOnCollectedOnHost(Network::Session::BoundComponent& boundComponent, Network::LocalClient& localClient);
		void OnCollectedInternal();

		[[nodiscard]] Entity::Component3D& GetOwner();
	protected:
		friend struct Reflection::ReflectedType<DestroySensor>;

		Optional<Audio::SoundSpotComponent*> m_pSoundSpotComponent;
		Asset::Guid m_audioAssetGuid;
		Entity::ComponentTemplateIdentifier m_sceneTemplateIdentifier;
		Optional<Entity::Component3D*> m_pMeshComponent;

		Threading::TimerHandle m_revertLocalCollectJobHandle;
		bool m_collectedOnHost{false};
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<GameFramework::DestroySensor>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::DestroySensor>(
			GameFramework::DestroySensor::TypeGuid,
			MAKE_UNICODE_LITERAL("Destroy Sensor"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Collect Sound"),
					"audio_asset",
					"{158b1273-5d60-4902-b805-b1ff42bb55a5}"_guid,
					MAKE_UNICODE_LITERAL("Destroy Sensor"),
					Reflection::PropertyFlags::VisibleToParentScope,
					&GameFramework::DestroySensor::SetAudioAsset,
					&GameFramework::DestroySensor::GetAudioAsset
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Mesh"),
					"mesh",
					"{ad707e5b-d352-4cb0-bfb5-8b8e41f07025}"_guid,
					MAKE_UNICODE_LITERAL("Destroy Sensor"),
					Reflection::PropertyFlags::VisibleToParentScope,
					&GameFramework::DestroySensor::SetSceneAsset,
					&GameFramework::DestroySensor::GetSceneAsset
				)
			},
			Reflection::Functions{Function{
				"{59c0d8b6-aa36-4500-868c-6b568c9f5895}"_guid,
				MAKE_UNICODE_LITERAL("On Triggered On Host"),
				&GameFramework::DestroySensor::ClientOnCollectedOnHost,
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
				"2620c4b6-a74e-427f-a036-8b8004557127"_asset
			}}
		);
	};
}
