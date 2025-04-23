#pragma once

#include "Components/SensorComponent.h"
#include "Components/Player/Score.h"

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
	struct CollectibleComponent final : public SensorComponent
	{
		static constexpr Guid TypeGuid = "97f5d985-8b59-4864-82d8-0856b85b8c68"_guid;

		using InstanceIdentifier = TIdentifier<uint32, 9>;

		using BaseType = SensorComponent;
		using BaseType::BaseType;

		CollectibleComponent(Initializer&& initializer);
		CollectibleComponent(const CollectibleComponent& templateComponent, const Cloner& cloner);

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
		friend struct Reflection::ReflectedType<CollectibleComponent>;

		Math::ClampedValue<int32> m_scoreReward = {1, -Score::MaximumScore, Score::MaximumScore};
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
	struct ReflectedType<GameFramework::CollectibleComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::CollectibleComponent>(
			GameFramework::CollectibleComponent::TypeGuid,
			MAKE_UNICODE_LITERAL("Collectible Sensor"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Score Reward"),
					"scoreReward",
					"{BFF4E48F-1AF1-43C7-8280-6A94D0EA42E5}"_guid,
					MAKE_UNICODE_LITERAL("Collectible"),
					Reflection::PropertyFlags::VisibleToParentScope,
					&GameFramework::CollectibleComponent::m_scoreReward
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Collect Sound"),
					"audio_asset",
					"{48B1C20E-B56D-4C50-A669-95E5CC6B8CD7}"_guid,
					MAKE_UNICODE_LITERAL("Collectible"),
					Reflection::PropertyFlags::VisibleToParentScope,
					&GameFramework::CollectibleComponent::SetAudioAsset,
					&GameFramework::CollectibleComponent::GetAudioAsset
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Mesh"),
					"mesh",
					"{4E9C7C35-179E-4A13-80F3-15705F226917}"_guid,
					MAKE_UNICODE_LITERAL("Collectible"),
					Reflection::PropertyFlags::VisibleToParentScope,
					&GameFramework::CollectibleComponent::SetSceneAsset,
					&GameFramework::CollectibleComponent::GetSceneAsset
				)
			},
			Reflection::Functions{Function{
				"{327F7165-2C88-4F6A-BA6A-257DEF3C7DA0}"_guid,
				MAKE_UNICODE_LITERAL("On Collected On Host"),
				&GameFramework::CollectibleComponent::ClientOnCollectedOnHost,
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
				"451fe5aa-a6c6-492e-93c1-8a93d879f48c"_asset
			}}
		);
	};
}
