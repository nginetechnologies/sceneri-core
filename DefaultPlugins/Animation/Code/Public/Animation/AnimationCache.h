#pragma once

#include "AnimationIdentifier.h"

#include <Common/Memory/UniquePtr.h>
#include <Common/Memory/Containers/InlineVector.h>
#include <Common/Function/Function.h>
#include <Common/Function/ThreadSafeEvent.h>
#include <Common/Storage/AtomicIdentifierMask.h>

#include <Engine/Asset/AssetType.h>

namespace ngine::Threading
{
	struct Job;
}

namespace ngine::Animation
{
	struct Animation;

	struct Info
	{
		Info();
		Info(UniquePtr<Animation>&& pAnimation);
		Info(Info&&);
		Info& operator=(Info&&);
		Info(const Info&) = delete;
		Info& operator=(const Info&) = delete;
		~Info();

		UniquePtr<Animation> m_pAnimation;
	};

	struct AnimationCache final : public Asset::Type<AnimationIdentifier, Info>
	{
		using BaseType = Type;

		AnimationCache(Asset::Manager& assetManager);
		AnimationCache(const AnimationCache&) = delete;
		AnimationCache& operator=(const AnimationCache&) = delete;
		AnimationCache(AnimationCache&&) = delete;
		AnimationCache& operator=(AnimationCache&&) = delete;
		virtual ~AnimationCache();

		[[nodiscard]] AnimationIdentifier FindOrRegisterAsset(const Asset::Guid guid);
		[[nodiscard]] AnimationIdentifier RegisterAsset(const Asset::Guid guid);

		[[nodiscard]] Optional<Animation*> GetAnimation(const AnimationIdentifier identifier) const;

		using AnimationLoadEvent = ThreadSafe::Event<void(void*, const AnimationIdentifier), 24, false>;
		using AnimationLoadListenerData = AnimationLoadEvent::ListenerData;
		using AnimationLoadListenerIdentifier = AnimationLoadEvent::ListenerIdentifier;

		[[nodiscard]] Threading::Job*
		TryLoadAnimation(const AnimationIdentifier identifier, Asset::Manager& assetManager, AnimationLoadListenerData&& listenerData);
		[[nodiscard]] bool
		RemoveAnimationListener(const AnimationIdentifier identifier, const AnimationLoadListenerIdentifier listenerIdentifier);

		[[nodiscard]] bool IsLoaded(const AnimationIdentifier identifier) const;
	protected:
#if DEVELOPMENT_BUILD
		virtual void OnAssetModified(const Asset::Guid assetGuid, const IdentifierType identifier, const IO::PathView filePath) override;
#endif
	protected:
		Threading::AtomicIdentifierMask<AnimationIdentifier> m_loadingAnimations;

		Threading::SharedMutex m_animationRequesterMutex;
		UnorderedMap<AnimationIdentifier, UniquePtr<AnimationLoadEvent>, AnimationIdentifier::Hash> m_animationRequesterMap;
	};
}
