#include "SkeletonCache.h"
#include "Skeleton.h"
#include "SkeletonAssetType.h"

#include <Engine/Asset/AssetType.inl>
#include <Common/Threading/Jobs/Job.h>
#include <Common/Threading/Jobs/JobBatch.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/Reflection/Registry.inl>
#include <Engine/Threading/JobRunnerThread.h>
#include <Common/System/Query.h>

#include <Common/Serialization/Reader.h>
#include <Common/Memory/AddressOf.h>
#include <Common/IO/Log.h>

namespace ngine::Animation
{
	SkeletonCache::SkeletonCache(Asset::Manager& assetManager)
	{
		RegisterAssetModifiedCallback(assetManager);
	}

	SkeletonCache::~SkeletonCache() = default;

	SkeletonIdentifier SkeletonCache::FindOrRegisterAsset(const Asset::Guid guid)
	{
		return BaseType::FindOrRegisterAsset(
			guid,
			[](const SkeletonIdentifier, const Asset::Guid)
			{
				return UniquePtr<Skeleton>{Memory::ConstructInPlace};
			}
		);
	}

	SkeletonIdentifier SkeletonCache::RegisterAsset(const Asset::Guid guid)
	{
		const SkeletonIdentifier identifier = BaseType::RegisterAsset(
			guid,
			[](const SkeletonIdentifier, const Guid)
			{
				return UniquePtr<Skeleton>{Memory::ConstructInPlace};
			}
		);
		return identifier;
	}

	Optional<Skeleton*> SkeletonCache::GetSkeleton(const SkeletonIdentifier identifier) const
	{
		return BaseType::GetAssetData(identifier).Get();
	}

	Threading::JobBatch
	SkeletonCache::TryLoadSkeleton(const SkeletonIdentifier identifier, Asset::Manager& assetManager, OnLoadedCallback&& callback)
	{
		SkeletonRequesters* pSkeletonRequesters;
		uint32 callbackIndex;
		{
			Threading::SharedLock readLock(m_skeletonRequesterMutex);
			decltype(m_skeletonRequesterMap)::iterator it = m_skeletonRequesterMap.Find(identifier);
			if (it != m_skeletonRequesterMap.end())
			{
				pSkeletonRequesters = it->second.Get();
			}
			else
			{
				readLock.Unlock();
				Threading::UniqueLock writeLock(m_skeletonRequesterMutex);
				it = m_skeletonRequesterMap.Find(identifier);
				if (it != m_skeletonRequesterMap.end())
				{
					pSkeletonRequesters = it->second.Get();
				}
				else
				{
					pSkeletonRequesters =
						m_skeletonRequesterMap.Emplace(SkeletonIdentifier(identifier), UniquePtr<SkeletonRequesters>::Make())->second.Get();
				}
			}

			Threading::UniqueLock requestersLock(pSkeletonRequesters->m_mutex);
			OnLoadedCallback& emplacedCallback = pSkeletonRequesters->m_callbacks.EmplaceBack(Forward<OnLoadedCallback>(callback));
			callbackIndex = pSkeletonRequesters->m_callbacks.GetIteratorIndex(Memory::GetAddressOf(emplacedCallback));
		}

		const Guid assetGuid = GetAssetGuid(identifier);
		Skeleton& skeleton = *GetAssetData(identifier);
		if (!skeleton.IsValid())
		{
			if (m_loadingSkeletons.Set(identifier))
			{
				if (!skeleton.IsValid())
				{
					Assert(m_loadingSkeletons.IsSet(identifier));
					Threading::Job* pBinaryLoadJob = assetManager.RequestAsyncLoadAssetBinary(
						assetGuid,
						Threading::JobPriority::LoadSkeleton,
						[&skeleton](const ConstByteView data)
						{
							Assert(data.HasElements());
							[[maybe_unused]] const bool wasLoaded = skeleton.Load(data);
							Assert(wasLoaded);
						}
					);
					Threading::Job* pMetadataLoadJob = assetManager.RequestAsyncLoadAssetMetadata(
						assetGuid,
						Threading::JobPriority::LoadSkeleton,
						[&skeleton, assetGuid](const ConstByteView data)
						{
							if (UNLIKELY(!data.HasElements()))
							{
								LogWarning("Skeleton data was empty when loading asset {0}!", assetGuid.ToString());
								return;
							}

							Serialization::Data skeletonData(
								ConstStringView{reinterpret_cast<const char*>(data.GetData()), static_cast<uint32>(data.GetDataSize() / sizeof(char))}
							);
							if (UNLIKELY(!skeletonData.IsValid()))
							{
								LogWarning("Skeleton was invalid when loading asset {0}!", assetGuid.ToString());
								return;
							}

							const Serialization::Reader skeletonReader(skeletonData);
							[[maybe_unused]] const bool wasLoaded = skeletonReader.SerializeInPlace(skeleton);

							LogWarningIf(!wasLoaded, "Skeleton asset {0} couldn't be loaded!", assetGuid.ToString());
						}
					);

					if (pBinaryLoadJob != nullptr || pMetadataLoadJob != nullptr)
					{
						Threading::JobBatch jobBatch(Threading::JobBatch::IntermediateStage);
						if (pBinaryLoadJob != nullptr)
						{
							jobBatch.QueueAfterStartStage(*pBinaryLoadJob);
						}
						if (pMetadataLoadJob != nullptr)
						{
							jobBatch.QueueAfterStartStage(*pMetadataLoadJob);
						}

						jobBatch.QueueAsNewFinishedStage(Threading::CreateCallback(
							[this, identifier, &skeleton](Threading::JobRunnerThread&)
							{
								skeleton.OnChanged();

								[[maybe_unused]] const bool wasCleared = m_loadingSkeletons.Clear(identifier);
								Assert(wasCleared);

								{
									Threading::SharedLock readLock(m_skeletonRequesterMutex);
									const decltype(m_skeletonRequesterMap)::const_iterator it = m_skeletonRequesterMap.Find(identifier);
									if (it != m_skeletonRequesterMap.end())
									{
										SkeletonRequesters& __restrict requesters = *it->second;
										Threading::SharedLock requestersReadLock(requesters.m_mutex);
										for (const OnLoadedCallback& callback : requesters.m_callbacks)
										{
											callback(identifier);
										}
									}
								}
							},
							Threading::JobPriority::LoadSkeleton
						));

						return jobBatch;
					}
				}
			}
		}

		if (skeleton.IsValid())
		{
			Threading::SharedLock requestersReadLock(pSkeletonRequesters->m_mutex);
			pSkeletonRequesters->m_callbacks[callbackIndex](identifier);
		}
		return {};
	}

#if DEVELOPMENT_BUILD
	void SkeletonCache::OnAssetModified(
		[[maybe_unused]] const Asset::Guid assetGuid, const IdentifierType identifier, [[maybe_unused]] const IO::PathView filePath
	)
	{
		Optional<Skeleton*> pSkeleton = GetSkeleton(identifier);
		if (pSkeleton.IsValid() && pSkeleton->IsValid())
		{
			if (m_loadingSkeletons.Set(identifier))
			{
				Threading::EngineJobRunnerThread& thread = static_cast<Threading::EngineJobRunnerThread&>(*Threading::JobRunnerThread::GetCurrent()
				);

				Asset::Manager& assetManager = System::Get<Asset::Manager>();
				Assert(m_loadingSkeletons.IsSet(identifier));
				Threading::Job* pLoadJob = assetManager.RequestAsyncLoadAssetBinary(
					assetGuid,
					Threading::JobPriority::LoadSkeleton,
					[&skeleton = *pSkeleton, &loadingSkeletons = m_loadingSkeletons, identifier](const ConstByteView data)
					{
						Skeleton newSkeleton;
						newSkeleton.Load(data);
						skeleton = Move(newSkeleton);

						[[maybe_unused]] const bool wasCleared = loadingSkeletons.Clear(identifier);
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

	[[maybe_unused]] const bool wasSkeletonAssetTypeRegistered = Reflection::Registry::RegisterType<SkeletonAssetType>();
}

namespace ngine
{
	template struct UnorderedMap<Guid, Animation::JointIndex, Guid::Hash>;
}
