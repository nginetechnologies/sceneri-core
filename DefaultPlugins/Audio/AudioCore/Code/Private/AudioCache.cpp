#include "AudioCache.h"
#include "AudioAsset.h"
#include "OpenAL/OpenALData.h"

#include <Engine/Asset/AssetType.inl>
#include <Engine/Asset/AssetManager.h>
#include <Engine/Threading/JobRunnerThread.h>

#include <Common/Threading/Jobs/Job.h>
#include <Common/Threading/Jobs/JobBatch.h>
#include <Common/Threading/Jobs/Job.h>
#include <Common/Threading/Jobs/JobBatch.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/Reflection/Registry.inl>
#include <Common/Memory/Containers/Format/StringView.h>
#include <Common/IO/Log.h>

namespace ngine::Audio
{
	Cache::Cache(Asset::Manager& assetManager)
	{
		RegisterAssetModifiedCallback(assetManager);
	}

	Identifier Cache::FindOrRegisterAsset(const Asset::Guid guid)
	{
		return BaseType::FindOrRegisterAsset(
			guid,
			[](const Identifier identifier, const Asset::Guid)
			{
				return UniquePtr<Audio::OpenALData>::Make(identifier);
			}
		);
	}

	Identifier Cache::RegisterAsset(const Asset::Guid guid)
	{
		const Identifier identifier = BaseType::RegisterAsset(
			guid,
			[](const Identifier identifier, const Guid)
			{
				return UniquePtr<Audio::OpenALData>::Make(identifier);
			}
		);
		return identifier;
	}

	Optional<Data*> Cache::FindAudio(const Identifier identifier) const
	{
		return &*BaseType::GetAssetData(identifier);
	}

	Optional<Threading::Job*>
	Cache::TryLoadAudio(const Identifier identifier, Asset::Manager& assetManager, AudioLoadListenerData&& newListenerData)
	{
		AudioLoadEvent* pAudioRequesters;

		{
			Threading::SharedLock readLock(m_audioRequesterMutex);
			decltype(m_audioRequesterMap)::iterator it = m_audioRequesterMap.Find(identifier);
			if (it != m_audioRequesterMap.end())
			{
				pAudioRequesters = it->second.Get();
			}
			else
			{
				readLock.Unlock();
				Threading::UniqueLock writeLock(m_audioRequesterMutex);
				it = m_audioRequesterMap.Find(identifier);
				if (it != m_audioRequesterMap.end())
				{
					pAudioRequesters = it->second.Get();
				}
				else
				{
					pAudioRequesters = m_audioRequesterMap.Emplace(Identifier(identifier), UniquePtr<AudioLoadEvent>::Make())->second.Get();
				}
			}
		}

		pAudioRequesters->Emplace(Forward<AudioLoadListenerData>(newListenerData));

		const Guid assetGuid = GetAssetGuid(identifier);
		Audio::Data& audioData = *GetAssetData(identifier);

		if (!audioData.IsValid())
		{
			if (m_loadedAudio.Set(identifier))
			{
				if (!audioData.IsValid())
				{
					return assetManager.RequestAsyncLoadAssetBinary(
						assetGuid,
						Threading::JobPriority::LoadAudio,
						[this, &audioData, assetGuid, identifier, pAudioRequesters](const ConstByteView data)
						{
							LogWarningIf(data.GetDataSize() == 0, "Audio asset data is empty {0}!", assetGuid.ToString());
							if (LIKELY(data.HasElements()))
							{
								bool wasLoaded = audioData.Load(data);
								LogWarningIf(!wasLoaded, "Audio asset {0} couldn't be loaded!", assetGuid.ToString());
							}

							[[maybe_unused]] const bool wasCleared = m_loadedAudio.Clear(identifier);
							Assert(wasCleared);

							(*pAudioRequesters)(identifier);
						}
					);
				}
				else
				{
					m_loadedAudio.Clear(identifier);
				}
			}
		}

		if (audioData.IsValid())
		{
			[[maybe_unused]] const bool wasExecuted = pAudioRequesters->Execute(newListenerData.m_identifier, identifier);
			Assert(wasExecuted);
		}
		return nullptr;
	}

	bool Cache::RemoveAudioListener(const Identifier identifier, const AudioLoadListenerIdentifier listenerIdentifier)
	{
		Threading::SharedLock readLock(m_audioRequesterMutex);
		decltype(m_audioRequesterMap)::iterator it = m_audioRequesterMap.Find(identifier);
		if (it != m_audioRequesterMap.end())
		{
			AudioLoadEvent* pAudioRequesters = it->second.Get();
			readLock.Unlock();

			return pAudioRequesters->Remove(listenerIdentifier);
		}

		return false;
	}

#if DEVELOPMENT_BUILD
	void Cache::OnAssetModified(
		[[maybe_unused]] const Asset::Guid assetGuid,
		[[maybe_unused]] const IdentifierType identifier,
		[[maybe_unused]] const IO::PathView filePath
	)
	{
		// TODO: Implement
	}
#endif
}
