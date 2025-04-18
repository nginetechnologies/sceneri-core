#pragma once

#include "AudioData.h"
#include "AudioIdentifier.h"

#include <Common/Memory/UniquePtr.h>
#include <Common/Memory/Containers/UnorderedMap.h>
#include <Common/Memory/Containers/InlineVector.h>
#include <Common/Storage/AtomicIdentifierMask.h>
#include <Common/Function/Function.h>
#include <Common/Function/ThreadSafeEvent.h>
#include <Common/Threading/Jobs/Job.h>

#include <Engine/Asset/AssetType.h>

namespace ngine::Threading
{
	struct JobBatch;
}

namespace ngine::Audio
{
	struct Cache final : public Asset::Type<Identifier, UniquePtr<Data>>
	{
		using BaseType = Type;

		Cache(Asset::Manager& assetManager);
		virtual ~Cache()
		{
		}

		[[nodiscard]] Identifier FindOrRegisterAsset(const Asset::Guid guid);
		[[nodiscard]] Identifier RegisterAsset(const Asset::Guid guid);

		[[nodiscard]] Optional<Data*> FindAudio(const Identifier identifier) const;

		using AudioLoadEvent = ThreadSafe::Event<EventCallbackResult(void*, const Identifier), 24>;
		using AudioLoadListenerData = AudioLoadEvent::ListenerData;
		using AudioLoadListenerIdentifier = AudioLoadEvent::ListenerIdentifier;

		[[nodiscard]] Optional<Threading::Job*>
		TryLoadAudio(const Identifier identifier, Asset::Manager& assetManager, AudioLoadListenerData&& listenerData);
		[[nodiscard]] bool RemoveAudioListener(const Identifier identifier, const AudioLoadListenerIdentifier listenerIdentifier);
	protected:
#if DEVELOPMENT_BUILD
		virtual void OnAssetModified(const Asset::Guid assetGuid, const IdentifierType identifier, const IO::PathView filePath) override;
#endif
	protected:
		Threading::AtomicIdentifierMask<Identifier> m_loadedAudio;

		Threading::SharedMutex m_audioRequesterMutex;
		UnorderedMap<Identifier, UniquePtr<AudioLoadEvent>, Identifier::Hash> m_audioRequesterMap;
	};
}
