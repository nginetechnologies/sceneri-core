#include "Components/SoundSpotComponent.h"

#include <Engine/Scene/Scene.h>
#include <Engine/Entity/ComponentRegistry.h>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/ComponentSoftReference.inl>
#include <Engine/Asset/AssetManager.h>
#include <Engine/Threading/JobManager.h>

#include "Plugin.h"
#include "AudioAsset.h"
#include "AudioData.h"
#include "AudioCache.h"
#include "AudioIdentifier.h"
#include "OpenAL/OpenALSource.h"

#include <Common/Reflection/Type.h>
#include <Common/Reflection/Registry.inl>
#include <Common/IO/Log.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>

namespace ngine::Audio
{
	SoundSpotComponent::SoundSpotComponent(const Deserializer& deserializer)
		: Entity::Component3D(deserializer)
	{
	}

	SoundSpotComponent::SoundSpotComponent(Initializer&& initializer)
		: Entity::Component3D(Forward<Initializer>(initializer))
		, m_flags(initializer.m_soundFlags)
		, m_volume(initializer.m_volume)
		, m_radius(initializer.m_radius)
	{
		Assert(m_radius.GetMeters() > 0.f);

		Cache& audioCache = System::FindPlugin<Audio::Plugin>()->GetCache();
		const Identifier identifier = audioCache.FindOrRegisterAsset(initializer.m_asset);
		m_pAudioData = audioCache.FindAudio(identifier);
	}

	SoundSpotComponent::SoundSpotComponent(const SoundSpotComponent& templateComponent, const Cloner& cloner)
		: Entity::Component3D(templateComponent, cloner)
		, m_pAudioData(templateComponent.m_pAudioData)
		, m_flags(templateComponent.m_flags & Flags::Settings)
		, m_volume(templateComponent.m_volume)
		, m_radius(templateComponent.m_radius)
	{
		Assert(m_radius.GetMeters() > 0.f);
	}

	SoundSpotComponent::~SoundSpotComponent()
	{
		if (m_pAudioData.IsValid())
		{
			Cache& audioCache = System::FindPlugin<Audio::Plugin>()->GetCache();
			const Identifier identifier = m_pAudioData->GetIdentifier();
			[[maybe_unused]] const bool wasRemoved = audioCache.RemoveAudioListener(identifier, this);
		}
	}

	void SoundSpotComponent::OnCreated()
	{
		if (m_pAudioData.IsValid() && m_flags.IsNotSet(Flags::HasRequestedLoad))
		{
			Threading::JobBatch jobBatch = LoadAudioSource();
			if (jobBatch.IsValid())
			{
				if (const Optional<Threading::JobRunnerThread*> pThread = Threading::JobRunnerThread::GetCurrent())
				{
					pThread->Queue(jobBatch);
				}
				else
				{
					System::Get<Threading::JobManager>().Queue(jobBatch, Threading::JobPriority::LoadAudio);
				}
			}
		}
	}

	Threading::JobBatch SoundSpotComponent::SetDeserializedSound(
		const Asset::Picker asset,
		[[maybe_unused]] const Serialization::Reader objectReader,
		[[maybe_unused]] const Serialization::Reader typeReader
	)
	{
		if (asset.GetAssetGuid().IsValid())
		{
			if (SetAudio(asset.GetAssetGuid()))
			{
				return LoadAudioSource();
			}
			else
			{
				return {};
			}
		}
		else
		{
			return {};
		}
	}

	bool SoundSpotComponent::SetAudio(const Asset::Guid assetGuid)
	{
		Cache& audioCache = System::FindPlugin<Audio::Plugin>()->GetCache();
		return SetAudio(audioCache.FindOrRegisterAsset(assetGuid));
	}

	bool SoundSpotComponent::SetAudio(const Identifier identifier)
	{
		Cache& audioCache = System::FindPlugin<Audio::Plugin>()->GetCache();
		if (m_pAudioData.IsValid())
		{
			if (identifier == m_pAudioData->GetIdentifier())
			{
				return false;
			}

			// Destroy previous source if there was one
			if (m_pAudioSource.IsValid())
			{
				if (IsEnabled())
				{
					m_pAudioSource->Stop();
				}

				m_pAudioSource.DestroyElement();
			}

			// Reset audio data handle - is owned by the asset system.
			[[maybe_unused]] const bool wasRemoved = audioCache.RemoveAudioListener(identifier, this);
			m_pAudioData = nullptr;
			m_flags.Clear(Flags::HasRequestedLoad);
		}

		m_pAudioData = audioCache.FindAudio(identifier);
		return true;
	}

	Threading::JobBatch SoundSpotComponent::LoadAudioSource()
	{
		Assert(m_pAudioSource.IsInvalid());
		Assert(m_pAudioData.IsValid());
		m_flags |= Flags::HasRequestedLoad;

		Cache& audioCache = System::FindPlugin<Audio::Plugin>()->GetCache();
		const Identifier identifier = audioCache.FindOrRegisterAsset(GetAudioAsset().GetAssetGuid());

		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		return audioCache.TryLoadAudio(
			identifier,
			System::Get<Asset::Manager>(),
			Cache::AudioLoadListenerData{
				*this,
				[&sceneRegistry,
		     softReference = Entity::ComponentSoftReference{*this, sceneRegistry}](const SoundSpotComponent&, const Audio::Identifier)
				{
					if (const Optional<SoundSpotComponent*> pSoundSpotComponent = softReference.Find<SoundSpotComponent>(sceneRegistry))
					{
						pSoundSpotComponent->OnLoaded();
					}
					return EventCallbackResult::Remove;
				}
			}
		);
	}

	void SoundSpotComponent::OnLoaded()
	{
		if (m_pAudioData.IsInvalid() || !m_pAudioData->IsLoaded())
		{
			LogError("Audio data was invalid while creating a source");
			return;
		}

		Assert(m_pAudioSource.IsInvalid());
		m_pAudioSource = UniquePtr<OpenALSource>::Make(*m_pAudioData);

		// Apply audio settings
		m_pAudioSource->SetPosition(GetWorldLocation());
		m_pAudioSource->SetLooping(m_flags.IsSet(Flags::Looping));
		m_pAudioSource->SetVolume(m_volume);
		m_pAudioSource->SetReferenceDistance(m_radius * 2.f);

		if (IsEnabled() && !GetRootScene().IsTemplate() && ShouldAutoplay())
		{
			m_pAudioSource->Play();
		}
	}

	Asset::Picker SoundSpotComponent::GetAudioAsset() const
	{
		if (m_pAudioData.IsValid())
		{
			Cache& audioCache = System::FindPlugin<Audio::Plugin>()->GetCache();
			return {audioCache.GetAssetGuid(m_pAudioData->GetIdentifier()), AssetFormat.assetTypeGuid};
		}
		else
		{
			return {Guid(), AssetFormat.assetTypeGuid};
		}
	}

	void SoundSpotComponent::SetAudioAsset(const Asset::Picker asset)
	{
		if (asset.GetAssetGuid().IsValid())
		{
			if (SetAudio(asset.GetAssetGuid()))
			{
				Threading::JobBatch jobBatch = LoadAudioSource();
				if (jobBatch.IsValid())
				{
					if (const Optional<Threading::JobRunnerThread*> pThread = Threading::JobRunnerThread::GetCurrent())
					{
						pThread->Queue(jobBatch);
					}
					else
					{
						System::Get<Threading::JobManager>().Queue(jobBatch, Threading::JobPriority::LoadAudio);
					}
				}
			}
		}
	}

	void SoundSpotComponent::SetLooping(bool shouldLoop)
	{
		m_flags.Set(Flags::Looping, shouldLoop);

		if (m_pAudioSource.IsValid())
		{
			m_pAudioSource->SetLooping(shouldLoop);
		}
	}

	void SoundSpotComponent::SetVolume(Volume volume)
	{
		m_volume = volume;

		if (m_pAudioSource.IsValid())
		{
			m_pAudioSource->SetVolume(volume);
		}
	}

	void SoundSpotComponent::SetRadius(const Math::Radiusf radius)
	{
		Assert(m_radius.GetMeters() > 0.f);
		m_radius = radius;

		if (m_pAudioSource.IsValid())
		{
			m_pAudioSource->SetReferenceDistance(radius * 2.f);
		}
	}

	void SoundSpotComponent::OnWorldTransformChanged([[maybe_unused]] const EnumFlags<Entity::TransformChangeFlags> flags)
	{
		if (m_pAudioSource.IsValid())
		{
			m_pAudioSource->SetPosition(GetWorldLocation());
		}
	}

	void SoundSpotComponent::Play()
	{
		if (m_pAudioSource.IsValid())
		{
			m_pAudioSource->Play();
		}
	}

	void SoundSpotComponent::Pause()
	{
		if (m_pAudioSource.IsValid())
		{
			m_pAudioSource->Pause();
		}
	}

	void SoundSpotComponent::Stop()
	{
		if (m_pAudioSource.IsValid())
		{
			m_pAudioSource->Stop();
		}
	}

	bool SoundSpotComponent::IsPlaying() const
	{
		if (m_pAudioSource.IsValid())
		{
			return m_pAudioSource->IsPlaying();
		}
		else
		{
			return false;
		}
	}

	void SoundSpotComponent::PlayDelayed(Time::Durationf delay)
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		System::Get<Threading::JobManager>().ScheduleAsync(
			delay,
			[&sceneRegistry, softReference = Entity::ComponentSoftReference{*this, sceneRegistry}](Threading::JobRunnerThread&)
			{
				if (const Optional<SoundSpotComponent*> pSoundSpotComponent = softReference.Find<SoundSpotComponent>(sceneRegistry))
				{
					if (pSoundSpotComponent->m_pAudioSource.IsValid())
					{
						pSoundSpotComponent->m_pAudioSource->Play();
					}
				}
			},
			Threading::JobPriority::LoadAudio
		);
	}

	void SoundSpotComponent::OnDisable()
	{
		if (m_pAudioSource.IsValid())
		{
			m_pAudioSource->Stop();
		}
	}

	[[maybe_unused]] const bool wasSoundSpotRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<SoundSpotComponent>>::Make());
	[[maybe_unused]] const bool wasSoundSpotTypeRegistered = Reflection::Registry::RegisterType<SoundSpotComponent>();
}
