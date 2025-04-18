#include "AudioCore/Components/AudioComponentFunctions.h"
#include "AudioCore/Components/SoundSpotComponent.h"
#include "AudioCore/AudioAssetType.h"

#include <Engine/Asset/AssetManager.h>
#include <Engine/Entity/Component3D.inl>
#include <Engine/Entity/RootSceneComponent.h>

#include <Common/Reflection/Registry.inl>
#include <Common/System/Query.h>

namespace ngine::Audio
{
	void PlaySoundSpot(Entity::Component3D& component)
	{
		Entity::SceneRegistry& sceneRegistry = component.GetSceneRegistry();
		if (const Optional<SoundSpotComponent*> pSoundSpotComponent = component.As<SoundSpotComponent>(sceneRegistry))
		{
			pSoundSpotComponent->Play();
		}
	}

	void PauseSoundSpot(Entity::Component3D& component)
	{
		Entity::SceneRegistry& sceneRegistry = component.GetSceneRegistry();
		if (const Optional<SoundSpotComponent*> pSoundSpotComponent = component.As<SoundSpotComponent>(sceneRegistry))
		{
			pSoundSpotComponent->Pause();
		}
	}

	void StopSoundSpot(Entity::Component3D& component)
	{
		Entity::SceneRegistry& sceneRegistry = component.GetSceneRegistry();
		if (const Optional<SoundSpotComponent*> pSoundSpotComponent = component.As<SoundSpotComponent>(sceneRegistry))
		{
			pSoundSpotComponent->Stop();
		}
	}

	bool IsSoundSpotPlaying(Entity::Component3D& component)
	{
		Entity::SceneRegistry& sceneRegistry = component.GetSceneRegistry();
		if (const Optional<SoundSpotComponent*> pSoundSpotComponent = component.As<SoundSpotComponent>(sceneRegistry))
		{
			return pSoundSpotComponent->IsPlaying();
		}
		else
		{
			return false;
		}
	}

	void SetSoundSpotAsset(Entity::Component3D& component, const Asset::Identifier audioAssetIdentifier)
	{
		Entity::SceneRegistry& sceneRegistry = component.GetSceneRegistry();
		if (const Optional<SoundSpotComponent*> pSoundSpotComponent = component.As<SoundSpotComponent>(sceneRegistry))
		{
			Asset::Manager& assetManager = System::Get<Asset::Manager>();
			pSoundSpotComponent->SetAudioAsset(Asset::Picker{assetManager.GetAssetGuid(audioAssetIdentifier), AssetFormat.assetTypeGuid});
		}
	}

	void SetSoundSpotLooping(Entity::Component3D& component, const bool isLooping)
	{
		Entity::SceneRegistry& sceneRegistry = component.GetSceneRegistry();
		if (const Optional<SoundSpotComponent*> pSoundSpotComponent = component.As<SoundSpotComponent>(sceneRegistry))
		{
			pSoundSpotComponent->SetLooping(isLooping);
		}
	}

	void SetSoundSpotAutoPlay(Entity::Component3D& component, const bool shouldAutoPlay)
	{
		Entity::SceneRegistry& sceneRegistry = component.GetSceneRegistry();
		if (const Optional<SoundSpotComponent*> pSoundSpotComponent = component.As<SoundSpotComponent>(sceneRegistry))
		{
			pSoundSpotComponent->SetAutoplay(shouldAutoPlay);
		}
	}

	void SetSoundSpotVolume(Entity::Component3D& component, const Math::Ratiof volume)
	{
		Entity::SceneRegistry& sceneRegistry = component.GetSceneRegistry();
		if (const Optional<SoundSpotComponent*> pSoundSpotComponent = component.As<SoundSpotComponent>(sceneRegistry))
		{
			pSoundSpotComponent->SetVolume(Volume{volume});
		}
	}

	void SetSoundSpotRadius(Entity::Component3D& component, const Math::Radiusf radius)
	{
		Entity::SceneRegistry& sceneRegistry = component.GetSceneRegistry();
		if (const Optional<SoundSpotComponent*> pSoundSpotComponent = component.As<SoundSpotComponent>(sceneRegistry))
		{
			pSoundSpotComponent->SetRadius(radius);
		}
	}

	[[maybe_unused]] inline static const bool wasPlaySoundSpotReflected = Reflection::Registry::RegisterGlobalFunction<&PlaySoundSpot>();
	[[maybe_unused]] inline static const bool wasPauseSoundSpotReflected = Reflection::Registry::RegisterGlobalFunction<&PauseSoundSpot>();
	[[maybe_unused]] inline static const bool wasStopSoundSpotReflected = Reflection::Registry::RegisterGlobalFunction<&StopSoundSpot>();
	[[maybe_unused]] inline static const bool wasIsSoundSpotPlayingReflected =
		Reflection::Registry::RegisterGlobalFunction<&IsSoundSpotPlaying>();
	[[maybe_unused]] inline static const bool wasSetSoundSpotAssetAssetReflected =
		Reflection::Registry::RegisterGlobalFunction<&SetSoundSpotAsset>();
	[[maybe_unused]] inline static const bool wasSetSoundSpotLoopingReflected =
		Reflection::Registry::RegisterGlobalFunction<&SetSoundSpotLooping>();
	[[maybe_unused]] inline static const bool wasSetSoundSpotAutoPlayReflected =
		Reflection::Registry::RegisterGlobalFunction<&SetSoundSpotAutoPlay>();
	[[maybe_unused]] inline static const bool wasSetSoundSpotVolumeReflected =
		Reflection::Registry::RegisterGlobalFunction<&SetSoundSpotVolume>();
	[[maybe_unused]] inline static const bool wasSetSoundSpotRadiusReflected =
		Reflection::Registry::RegisterGlobalFunction<&SetSoundSpotRadius>();
}
