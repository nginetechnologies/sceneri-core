#pragma once

#include <Renderer/Assets/Material/MaterialIdentifier.h>
#include <Renderer/Assets/Material/MaterialInstanceIdentifier.h>

#include <Common/Asset/Guid.h>
#include <Common/Storage/SaltedIdentifierStorage.h>
#include <Common/Storage/AtomicIdentifierMask.h>
#include <Common/Memory/Containers/UnorderedMap.h>
#include <Common/Memory/UniquePtr.h>
#include <Engine/Asset/AssetType.h>
#include <Common/Function/ThreadSafeEvent.h>

namespace ngine::Threading
{
	struct JobBatch;
}

namespace ngine::Asset
{
	struct Manager;
}

namespace ngine::Rendering
{
	struct Renderer;
	struct RuntimeMaterial;
	struct RuntimeMaterialInstance;
	struct RuntimeDescriptorContent;
	struct MaterialCache;
	struct LogicalDevice;

	using MaterialInstanceLoadingCallback = Function<Threading::JobBatch(const MaterialInstanceIdentifier identifier), 8>;
	struct MaterialInstanceInfo
	{
		MaterialInstanceInfo(
			UniquePtr<RuntimeMaterialInstance>&& pMaterialInstance = Invalid, MaterialInstanceLoadingCallback&& loadingCallback = {}
		);
		MaterialInstanceInfo(MaterialInstanceInfo&&);
		MaterialInstanceInfo& operator=(MaterialInstanceInfo&&) = delete;
		MaterialInstanceInfo(const MaterialInstanceInfo&) = delete;
		MaterialInstanceInfo& operator=(const MaterialInstanceInfo&) = delete;
		~MaterialInstanceInfo();

		UniquePtr<RuntimeMaterialInstance> m_pMaterialInstance;
		MaterialInstanceLoadingCallback m_loadingCallback;
	};

	struct MaterialInstanceCache final : public Asset::Type<MaterialInstanceIdentifier, MaterialInstanceInfo>
	{
		using BaseType = Type;

		[[nodiscard]] PURE_LOCALS_AND_POINTERS MaterialCache& GetMaterialCache();
		[[nodiscard]] PURE_LOCALS_AND_POINTERS const MaterialCache& GetMaterialCache() const;

		MaterialInstanceCache();
		MaterialInstanceCache(const MaterialInstanceCache&) = delete;
		MaterialInstanceCache(MaterialInstanceCache&&) = delete;
		MaterialInstanceCache& operator=(const MaterialInstanceCache&) = delete;
		MaterialInstanceCache& operator=(MaterialInstanceCache&&) = delete;
		~MaterialInstanceCache();

		[[nodiscard]] MaterialInstanceIdentifier Clone(const MaterialInstanceIdentifier templateIdentifier);
		[[nodiscard]] MaterialInstanceIdentifier
		Create(const MaterialIdentifier materialIdentifier, RuntimeDescriptorContent&& descriptorContents);
		[[nodiscard]] MaterialInstanceIdentifier FindOrRegisterAsset(const Asset::Guid guid);
		[[nodiscard]] Optional<RuntimeMaterialInstance*> GetMaterialInstance(const MaterialInstanceIdentifier identifier) const
		{
			return GetAssetData(identifier).m_pMaterialInstance;
		}

		using OnLoadedEvent = ThreadSafe::Event<EventCallbackResult(void*), 24>;
		using OnLoadedListenerData = OnLoadedEvent::ListenerData;
		using OnLoadedListenerIdentifier = OnLoadedEvent::ListenerIdentifier;

		[[nodiscard]] Threading::JobBatch TryLoad(const MaterialInstanceIdentifier, OnLoadedListenerData&& listenerData = {});
		[[nodiscard]] bool
		RemoveOnLoadListener(const MaterialInstanceIdentifier identifier, const OnLoadedListenerIdentifier listenerIdentifier);
		[[nodiscard]] bool IsLoading(const MaterialInstanceIdentifier identifier) const
		{
			return m_loadingMaterialInstances.IsSet(identifier);
		}
	protected:
		void OnLoadingFinished(const MaterialInstanceIdentifier identifier);
		void OnLoadingFailed(const MaterialInstanceIdentifier identifier);
	protected:
		Threading::AtomicIdentifierMask<MaterialInstanceIdentifier> m_loadingMaterialInstances;

		Threading::SharedMutex m_instanceRequesterMutex;
		struct InstanceRequesters
		{
			OnLoadedEvent m_onLoaded;
		};

		UnorderedMap<MaterialInstanceIdentifier, UniqueRef<InstanceRequesters>, MaterialInstanceIdentifier::Hash> m_instanceRequesterMap;
	};

	using MaterialLoadingCallback = Function<Threading::JobBatch(const MaterialIdentifier identifier), 8>;
	struct MaterialInfo
	{
		MaterialInfo(UniquePtr<RuntimeMaterial>&& pMaterial = Invalid, MaterialLoadingCallback&& loadingCallback = {});
		MaterialInfo(MaterialInfo&&);
		MaterialInfo& operator=(MaterialInfo&&) = delete;
		MaterialInfo(const MaterialInfo&) = delete;
		MaterialInfo& operator=(const MaterialInfo&) = delete;
		~MaterialInfo();

		UniquePtr<RuntimeMaterial> m_pMaterial;
		MaterialLoadingCallback m_loadingCallback;
	};

	struct MaterialCache final : public Asset::Type<MaterialIdentifier, MaterialInfo>
	{
		using BaseType = Type;

		[[nodiscard]] PURE_LOCALS_AND_POINTERS Rendering::Renderer& GetRenderer();
		[[nodiscard]] PURE_LOCALS_AND_POINTERS const Rendering::Renderer& GetRenderer() const;

		MaterialCache();
		MaterialCache(const MaterialCache&) = delete;
		MaterialCache(MaterialCache&&);
		MaterialCache& operator=(const MaterialCache&) = delete;
		MaterialCache& operator=(MaterialCache&&) = delete;
		~MaterialCache();

		[[nodiscard]] MaterialIdentifier FindOrRegisterAsset(const Asset::Guid guid);

		[[nodiscard]] Optional<RuntimeMaterial*> GetMaterial(const MaterialIdentifier identifier) const
		{
			return GetAssetData(identifier).m_pMaterial;
		}

		[[nodiscard]] PURE_LOCALS_AND_POINTERS MaterialInstanceCache& GetInstanceCache()
		{
			return m_instanceCache;
		}
		[[nodiscard]] PURE_LOCALS_AND_POINTERS const MaterialInstanceCache& GetInstanceCache() const
		{
			return m_instanceCache;
		}

		using OnLoadedEvent = ThreadSafe::Event<EventCallbackResult(void*), 24, false>;
		using OnLoadedListenerData = OnLoadedEvent::ListenerData;
		using OnLoadedListenerIdentifier = OnLoadedEvent::ListenerIdentifier;

		[[nodiscard]] Threading::JobBatch TryLoad(const MaterialIdentifier, OnLoadedListenerData&& listenerData = {});
		[[nodiscard]] bool RemoveOnLoadListener(const MaterialIdentifier identifier, const OnLoadedListenerIdentifier listenerIdentifier);
		[[nodiscard]] bool IsLoading(const MaterialIdentifier identifier) const
		{
			return m_loadingMaterials.IsSet(identifier);
		}
	protected:
		void OnLoadingFinished(const MaterialIdentifier identifier);
		void OnLoadingFailed(const MaterialIdentifier identifier);
	protected:
		Threading::AtomicIdentifierMask<MaterialIdentifier> m_loadingMaterials;

		Threading::SharedMutex m_instanceRequesterMutex;
		struct InstanceRequesters
		{
			OnLoadedEvent m_onLoaded;
		};

		UnorderedMap<MaterialIdentifier, UniqueRef<InstanceRequesters>, MaterialIdentifier::Hash> m_instanceRequesterMap;

		friend MaterialInstanceCache;
		MaterialInstanceCache m_instanceCache;
	};
}
