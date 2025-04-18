#pragma once

#include "MeshSkinIdentifier.h"

#include <Common/Memory/UniquePtr.h>
#include <Common/Memory/Containers/UnorderedMap.h>
#include <Common/Memory/Containers/InlineVector.h>
#include <Common/Function/ThreadSafeEvent.h>
#include <Common/Storage/AtomicIdentifierMask.h>

#include <Engine/Asset/AssetType.h>

namespace ngine::Threading
{
	struct Job;
}

namespace ngine::Animation
{
	struct MeshSkin;

	struct MeshSkinData
	{
		MeshSkinData();
		MeshSkinData(UniquePtr<MeshSkin>&& pMeshSkin);
		MeshSkinData(const MeshSkinData&) = delete;
		MeshSkinData& operator=(const MeshSkinData&) = delete;
		MeshSkinData(MeshSkinData&&);
		MeshSkinData& operator=(MeshSkinData&&);
		~MeshSkinData();

		UniquePtr<MeshSkin> m_pMeshSkin;
	};

	struct MeshSkinCache final : public Asset::Type<MeshSkinIdentifier, MeshSkinData>
	{
		using BaseType = Type;

		MeshSkinCache(Asset::Manager& assetManager);
		MeshSkinCache(const MeshSkinCache&) = delete;
		MeshSkinCache& operator=(const MeshSkinCache&) = delete;
		MeshSkinCache(MeshSkinCache&&) = delete;
		MeshSkinCache& operator=(MeshSkinCache&&) = delete;
		virtual ~MeshSkinCache();

		[[nodiscard]] MeshSkinIdentifier FindOrRegisterAsset(const Asset::Guid guid);
		[[nodiscard]] MeshSkinIdentifier RegisterAsset(const Asset::Guid guid);

		[[nodiscard]] Optional<MeshSkin*> GetMeshSkin(const MeshSkinIdentifier identifier) const;

		using LoadEvent = ThreadSafe::Event<void(void*, const MeshSkinIdentifier), 24>;
		using LoadListenerData = LoadEvent::ListenerData;
		using ListenerIdentifier = LoadEvent::ListenerIdentifier;

		[[nodiscard]] Threading::Job*
		TryLoadMeshSkin(const MeshSkinIdentifier identifier, Asset::Manager& assetManager, LoadListenerData&& listenerData);
		bool RemoveListener(const MeshSkinIdentifier identifier, const ListenerIdentifier listenerIdentifier);
	protected:
#if DEVELOPMENT_BUILD
		virtual void OnAssetModified(const Asset::Guid assetGuid, const IdentifierType identifier, const IO::PathView filePath) override;
#endif
	protected:
		Threading::AtomicIdentifierMask<MeshSkinIdentifier> m_loadingMeshSkins;

		struct MeshSkinRequesters
		{
			LoadEvent m_onLoadedCallback;
		};

		Threading::SharedMutex m_meshSkinRequesterMutex;
		UnorderedMap<MeshSkinIdentifier, UniqueRef<MeshSkinRequesters>, MeshSkinIdentifier::Hash> m_meshSkinRequesterMap;
	};
}
