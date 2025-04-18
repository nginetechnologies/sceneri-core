#include "MeshSkinCache.h"
#include "MeshSkin.h"

#include <Engine/Asset/AssetType.inl>
#include <Common/Threading/Jobs/Job.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Engine/Threading/JobRunnerThread.h>
#include <Common/System/Query.h>
#define OZZ_INCLUDE_PRIVATE_HEADER 1
#include <3rdparty/ozz/sample_framework/mesh.h>
#include <3rdparty/ozz/animation/runtime/skeleton.h>
#include <3rdparty/ozz/animation/runtime/animation_keyframe.h>
#include <3rdparty/ozz/base/io/archive.h>
#include <3rdparty/ozz/base/io/stream.h>
#include <3rdparty/ozz/base/maths/soa_transform.h>
#include "Wrappers/FileStream.h"
#include <Renderer/Assets/StaticMesh/StaticObject.h>

#include "Skeleton.h"
#include "Animation.h"

#include <Common/Memory/AddressOf.h>
#include <Common/Math/Quaternion.h>
#include <Common/Math/ScaledQuaternion.h>

namespace ngine::Animation
{
	MeshSkinData::MeshSkinData() = default;
	MeshSkinData::MeshSkinData(UniquePtr<MeshSkin>&& pMeshSkin)
		: m_pMeshSkin(Forward<UniquePtr<MeshSkin>>(pMeshSkin))
	{
	}
	MeshSkinData::MeshSkinData(MeshSkinData&&) = default;
	MeshSkinData& MeshSkinData::operator=(MeshSkinData&&) = default;
	MeshSkinData::~MeshSkinData() = default;

	MeshSkinCache::MeshSkinCache(Asset::Manager& assetManager)
	{
		RegisterAssetModifiedCallback(assetManager);
	}

	MeshSkinCache::~MeshSkinCache() = default;

	MeshSkinIdentifier MeshSkinCache::FindOrRegisterAsset(const Asset::Guid guid)
	{
		return BaseType::FindOrRegisterAsset(
			guid,
			[](const MeshSkinIdentifier, const Asset::Guid)
			{
				return MeshSkinData{UniquePtr<MeshSkin>{Memory::ConstructInPlace}};
			}
		);
	}

	MeshSkinIdentifier MeshSkinCache::RegisterAsset(const Asset::Guid guid)
	{
		const MeshSkinIdentifier identifier = BaseType::RegisterAsset(
			guid,
			[](const MeshSkinIdentifier, const Guid)
			{
				return MeshSkinData{UniquePtr<MeshSkin>{Memory::ConstructInPlace}};
			}
		);
		return identifier;
	}

	Optional<MeshSkin*> MeshSkinCache::GetMeshSkin(const MeshSkinIdentifier identifier) const
	{
		return BaseType::GetAssetData(identifier).m_pMeshSkin;
	}

	Threading::Job*
	MeshSkinCache::TryLoadMeshSkin(const MeshSkinIdentifier identifier, Asset::Manager& assetManager, LoadListenerData&& newListenerData)
	{
		auto getRequesters = [this, identifier]() -> MeshSkinRequesters&
		{
			{
				Threading::SharedLock readLock(m_meshSkinRequesterMutex);
				decltype(m_meshSkinRequesterMap)::iterator it = m_meshSkinRequesterMap.Find(identifier);
				if (it != m_meshSkinRequesterMap.end())
				{
					return it->second;
				}
				else
				{
					readLock.Unlock();
					Threading::UniqueLock writeLock(m_meshSkinRequesterMutex);
					it = m_meshSkinRequesterMap.Find(identifier);
					if (it != m_meshSkinRequesterMap.end())
					{
						return it->second;
					}
					else
					{
						return m_meshSkinRequesterMap.Emplace(MeshSkinIdentifier(identifier), UniqueRef<MeshSkinRequesters>::Make())->second;
					}
				}
			}
		};
		MeshSkinRequesters& requesters = getRequesters();
		requesters.m_onLoadedCallback.Emplace(Forward<LoadListenerData>(newListenerData));

		const Guid assetGuid = GetAssetGuid(identifier);
		MeshSkin& meshSkin = *GetAssetData(identifier).m_pMeshSkin;
		if (meshSkin.IsValid())
		{
			[[maybe_unused]] const bool wasExecuted = requesters.m_onLoadedCallback.Execute(newListenerData.m_identifier, identifier);
			Assert(wasExecuted);
		}
		else
		{
			if (m_loadingMeshSkins.Set(identifier))
			{
				if (meshSkin.IsValid())
				{
					// Do nothing, mesh requester will have been used
					[[maybe_unused]] const bool cleared = m_loadingMeshSkins.Clear(identifier);
					Assert(cleared);

					[[maybe_unused]] const bool wasExecuted = requesters.m_onLoadedCallback.Execute(newListenerData.m_identifier, identifier);
					Assert(wasExecuted);
				}
				else
				{
					return assetManager.RequestAsyncLoadAssetBinary(
						assetGuid,
						Threading::JobPriority::LoadMeshSkin,
						[this, &meshSkin, identifier](const ConstByteView data)
						{
							Assert(data.HasElements());
							if (LIKELY(data.HasElements()))
							{
								meshSkin = MeshSkin(data);
								Assert(meshSkin.IsValid());
							}

							[[maybe_unused]] const bool wasCleared = m_loadingMeshSkins.Clear(identifier);
							Assert(wasCleared);

							{
								Threading::SharedLock readLock(m_meshSkinRequesterMutex);
								const decltype(m_meshSkinRequesterMap)::const_iterator it = m_meshSkinRequesterMap.Find(identifier);
								if (it != m_meshSkinRequesterMap.end())
								{
									MeshSkinRequesters& __restrict requesters = *it->second;
									requesters.m_onLoadedCallback(identifier);
								}
							}
						}
					);
				}
			}
		}

		return nullptr;
	}

	bool MeshSkinCache::RemoveListener(const MeshSkinIdentifier identifier, const ListenerIdentifier listenerIdentifier)
	{
		Threading::SharedLock readLock(m_meshSkinRequesterMutex);
		decltype(m_meshSkinRequesterMap)::iterator it = m_meshSkinRequesterMap.Find(identifier);
		if (it != m_meshSkinRequesterMap.end())
		{
			MeshSkinRequesters& meshSkinRequesters = *it->second;
			return meshSkinRequesters.m_onLoadedCallback.Remove(listenerIdentifier);
		}
		else
		{
			return false;
		}
	}

#if DEVELOPMENT_BUILD
	void MeshSkinCache::OnAssetModified(
		[[maybe_unused]] const Asset::Guid assetGuid, const IdentifierType identifier, [[maybe_unused]] const IO::PathView filePath
	)
	{
		Optional<MeshSkin*> pMeshSkin = GetMeshSkin(identifier);
		if (pMeshSkin.IsValid() && pMeshSkin->IsValid())
		{
			if (m_loadingMeshSkins.Set(identifier))
			{
				Threading::EngineJobRunnerThread& thread = static_cast<Threading::EngineJobRunnerThread&>(*Threading::JobRunnerThread::GetCurrent()
				);

				Asset::Manager& assetManager = System::Get<Asset::Manager>();
				Assert(m_loadingMeshSkins.IsSet(identifier));
				Threading::Job* pLoadJob = assetManager.RequestAsyncLoadAssetBinary(
					assetGuid,
					Threading::JobPriority::LoadMeshSkin,
					[&meshSkin = *pMeshSkin, &loadingSkeletons = m_loadingMeshSkins, identifier](const ConstByteView data)
					{
						meshSkin = MeshSkin(data);

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
}
