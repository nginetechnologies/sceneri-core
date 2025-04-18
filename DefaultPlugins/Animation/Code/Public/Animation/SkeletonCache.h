#pragma once

#include "SkeletonIdentifier.h"

#include <Common/Memory/UniquePtr.h>
#include <Common/Memory/Containers/UnorderedMap.h>
#include <Common/Memory/Containers/InlineVector.h>
#include <Common/Function/Function.h>
#include <Common/Storage/AtomicIdentifierMask.h>

#include <Engine/Asset/AssetType.h>

namespace ngine::Threading
{
	struct JobBatch;
}

namespace ngine::Animation
{
	struct Skeleton;

	struct SkeletonCache final : public Asset::Type<SkeletonIdentifier, UniquePtr<Skeleton>>
	{
		using BaseType = Type;

		SkeletonCache(Asset::Manager& assetManager);
		virtual ~SkeletonCache();

		[[nodiscard]] SkeletonIdentifier FindOrRegisterAsset(const Asset::Guid guid);
		[[nodiscard]] SkeletonIdentifier RegisterAsset(const Asset::Guid guid);

		[[nodiscard]] Optional<Skeleton*> GetSkeleton(const SkeletonIdentifier identifier) const;

		using OnLoadedCallback = Function<void(const SkeletonIdentifier), 24>;
		[[nodiscard]] Threading::JobBatch
		TryLoadSkeleton(const SkeletonIdentifier identifier, Asset::Manager& assetManager, OnLoadedCallback&& callback);
	protected:
#if DEVELOPMENT_BUILD
		virtual void OnAssetModified(const Asset::Guid assetGuid, const IdentifierType identifier, const IO::PathView filePath) override;
#endif
	protected:
		Threading::AtomicIdentifierMask<SkeletonIdentifier> m_loadingSkeletons;

		struct SkeletonRequesters
		{
			Threading::SharedMutex m_mutex;
			InlineVector<OnLoadedCallback, 2> m_callbacks;
		};

		Threading::SharedMutex m_skeletonRequesterMutex;
		UnorderedMap<SkeletonIdentifier, UniquePtr<SkeletonRequesters>, SkeletonIdentifier::Hash> m_skeletonRequesterMap;
	};
}
