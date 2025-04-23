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
	struct HealthCollectibleComponent final : public SensorComponent
	{
		static constexpr Guid TypeGuid = "67b6e2f9-bedc-4dc7-87ff-2ab7b27ab463"_guid;

		using InstanceIdentifier = TIdentifier<uint32, 9>;

		using BaseType = SensorComponent;
		using BaseType::BaseType;

		HealthCollectibleComponent(Initializer&& initializer);
		HealthCollectibleComponent(const HealthCollectibleComponent& templateComponent, const Cloner& cloner);

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

		void ClientOnCollectedOnHost(Network::Session::BoundComponent& boundComponent, Network::LocalClient& localClient);
		void OnCollectedInternal();

		[[nodiscard]] Entity::Component3D& GetOwner();
	protected:
		friend struct Reflection::ReflectedType<HealthCollectibleComponent>;

		Math::ClampedValue<Health::ValueType> m_healthReward = {1, 0, Math::NumericLimits<Health::ValueType>::Max};
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
	struct ReflectedType<GameFramework::HealthCollectibleComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::HealthCollectibleComponent>(
			GameFramework::HealthCollectibleComponent::TypeGuid,
			MAKE_UNICODE_LITERAL("Health Collectible Sensor"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Health Reward"),
					"healthReward",
					"{E651D224-B558-43AD-8F2E-C32A0EC59FF2}"_guid,
					MAKE_UNICODE_LITERAL("Health Collectible"),
					Reflection::PropertyFlags::VisibleToParentScope,
					&GameFramework::HealthCollectibleComponent::m_healthReward
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Collect Sound"),
					"audio_asset",
					"{0C6CFDFE-9EBE-4FBB-A6AD-1DEC179FEDEC}"_guid,
					MAKE_UNICODE_LITERAL("Health Collectible"),
					Reflection::PropertyFlags::VisibleToParentScope,
					&GameFramework::HealthCollectibleComponent::SetAudioAsset,
					&GameFramework::HealthCollectibleComponent::GetAudioAsset
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Mesh"),
					"mesh",
					"{8DD0670B-EFE6-4A97-9F07-60A4AB988F17}"_guid,
					MAKE_UNICODE_LITERAL("Health Collectible"),
					Reflection::PropertyFlags::VisibleToParentScope,
					&GameFramework::HealthCollectibleComponent::SetSceneAsset,
					&GameFramework::HealthCollectibleComponent::GetSceneAsset
				)
			},
			Reflection::Functions{Function{
				"{7961239D-5318-4F7E-B2DD-54C6372F0393}"_guid,
				MAKE_UNICODE_LITERAL("On Collected On Host"),
				&GameFramework::HealthCollectibleComponent::ClientOnCollectedOnHost,
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
				"3a7d2cff-eb77-4264-9564-e71b84f93b3e"_asset
			}}
		);
	};
}
