#include "AnimationCache.h"
#include "Animation.h"

#include <Engine/Asset/AssetType.inl>
#include <Common/Threading/Jobs/Job.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Engine/Threading/JobRunnerThread.h>
#include <Common/System/Query.h>

namespace ngine::Animation
{
	Info::Info() = default;
	Info::Info(UniquePtr<Animation>&& pAnimation)
		: m_pAnimation(Forward<UniquePtr<Animation>>(pAnimation))
	{
	}
	Info::Info(Info&&) = default;
	Info& Info::operator=(Info&&) = default;
	Info::~Info() = default;

	AnimationCache::AnimationCache(Asset::Manager& assetManager)
	{
		RegisterAssetModifiedCallback(assetManager);
	}

	AnimationCache::~AnimationCache() = default;

	AnimationIdentifier AnimationCache::FindOrRegisterAsset(const Asset::Guid guid)
	{
		return BaseType::FindOrRegisterAsset(
			guid,
			[](const AnimationIdentifier, const Asset::Guid)
			{
				return Info{UniquePtr<Animation>{Memory::ConstructInPlace}};
			}
		);
	}

	AnimationIdentifier AnimationCache::RegisterAsset(const Asset::Guid guid)
	{
		const AnimationIdentifier identifier = BaseType::RegisterAsset(
			guid,
			[](const AnimationIdentifier, const Guid)
			{
				return Info{UniquePtr<Animation>{Memory::ConstructInPlace}};
			}
		);
		return identifier;
	}

	Optional<Animation*> AnimationCache::GetAnimation(const AnimationIdentifier identifier) const
	{
		return BaseType::GetAssetData(identifier).m_pAnimation.Get();
	}

	Threading::Job* AnimationCache::TryLoadAnimation(
		const AnimationIdentifier identifier, Asset::Manager& assetManager, AnimationLoadListenerData&& newListenerData
	)
	{
		AnimationLoadEvent* pAnimationRequesters;

		{
			Threading::SharedLock readLock(m_animationRequesterMutex);
			decltype(m_animationRequesterMap)::iterator it = m_animationRequesterMap.Find(identifier);
			if (it != m_animationRequesterMap.end())
			{
				pAnimationRequesters = it->second.Get();
			}
			else
			{
				readLock.Unlock();
				Threading::UniqueLock writeLock(m_animationRequesterMutex);
				it = m_animationRequesterMap.Find(identifier);
				if (it != m_animationRequesterMap.end())
				{
					pAnimationRequesters = it->second.Get();
				}
				else
				{
					pAnimationRequesters =
						m_animationRequesterMap.Emplace(AnimationIdentifier(identifier), UniquePtr<AnimationLoadEvent>::Make())->second.Get();
				}
			}
		}

		pAnimationRequesters->Emplace(Forward<AnimationLoadListenerData>(newListenerData));

		Animation& animation = *GetAssetData(identifier).m_pAnimation;
		if (!animation.IsValid())
		{
			if (m_loadingAnimations.Set(identifier))
			{
				if (!animation.IsValid())
				{
					Assert(m_loadingAnimations.IsSet(identifier));
					const Guid assetGuid = GetAssetGuid(identifier);
					return assetManager.RequestAsyncLoadAssetBinary(
						assetGuid,
						Threading::JobPriority::LoadAnimation,
						[this, &animation, identifier, pAnimationRequesters](const ConstByteView data)
						{
							Assert(data.HasElements());
							if (LIKELY(data.HasElements()))
							{
								[[maybe_unused]] const bool wasLoaded = animation.Load(data);
								Assert(wasLoaded);
								Assert(animation.IsValid());
							}

							[[maybe_unused]] const bool wasCleared = m_loadingAnimations.Clear(identifier);
							Assert(wasCleared);

							(*pAnimationRequesters)(identifier);
						}
					);
				}
				else
				{
					m_loadingAnimations.Clear(identifier);
				}
			}
		}

		if (animation.IsValid())
		{
			[[maybe_unused]] const bool wasExecuted = pAnimationRequesters->Execute(newListenerData.m_identifier, identifier);
			Assert(wasExecuted);
		}
		return nullptr;
	}

	bool
	AnimationCache::RemoveAnimationListener(const AnimationIdentifier identifier, const AnimationLoadListenerIdentifier listenerIdentifier)
	{
		Threading::SharedLock readLock(m_animationRequesterMutex);
		decltype(m_animationRequesterMap)::iterator it = m_animationRequesterMap.Find(identifier);
		if (it != m_animationRequesterMap.end())
		{
			AnimationLoadEvent* pAnimationRequesters = it->second.Get();
			readLock.Unlock();

			return pAnimationRequesters->Remove(listenerIdentifier);
		}

		return false;
	}

	bool AnimationCache::IsLoaded(const AnimationIdentifier identifier) const
	{
		if (m_loadingAnimations.IsSet(identifier))
		{
			return false;
		}
		const Optional<Animation*> pAnimation = GetAssetData(identifier).m_pAnimation.Get();
		return pAnimation.IsValid() && pAnimation->IsValid();
	}

#if DEVELOPMENT_BUILD
	void AnimationCache::OnAssetModified(
		[[maybe_unused]] const Asset::Guid assetGuid, const IdentifierType identifier, [[maybe_unused]] const IO::PathView filePath
	)
	{
		Optional<Animation*> pAnimation = GetAnimation(identifier);
		if (pAnimation.IsValid() && pAnimation->IsValid())
		{
			if (m_loadingAnimations.Set(identifier))
			{
				Threading::EngineJobRunnerThread& thread = static_cast<Threading::EngineJobRunnerThread&>(*Threading::JobRunnerThread::GetCurrent()
				);

				Asset::Manager& assetManager = System::Get<Asset::Manager>();
				Assert(m_loadingAnimations.IsSet(identifier));
				Threading::Job* pLoadJob = assetManager.RequestAsyncLoadAssetBinary(
					assetGuid,
					Threading::JobPriority::LoadAnimation,
					[&animation = *pAnimation, &loadingAnimations = m_loadingAnimations, identifier](const ConstByteView data)
					{
						animation.Load(data);

						[[maybe_unused]] const bool wasCleared = loadingAnimations.Clear(identifier);
						Assert(wasCleared);
					}
				);
				pLoadJob->AddSubsequentStage(Threading::CreateCallback(
					[this, identifier](Threading::JobRunnerThread&)
					{
						[[maybe_unused]] const bool wasCleared = m_reloadingAssets.Clear(identifier);
						Assert(wasCleared);
					},
					Threading::JobPriority::FileChangeDetection
				));
				pLoadJob->Queue(thread);
			}
		}
	}
#endif
}
