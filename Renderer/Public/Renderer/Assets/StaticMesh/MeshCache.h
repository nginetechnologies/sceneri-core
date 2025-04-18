#pragma once

#include "StaticMeshIdentifier.h"

#include <Common/EnumFlags.h>
#include <Common/Memory/UniquePtr.h>
#include <Common/Memory/Optional.h>
#include <Common/Math/Primitives/BoundingBox.h>
#include <Common/Function/Function.h>
#include <Common/Function/ThreadSafeEvent.h>
#include <Common/Memory/ReferenceWrapper.h>
#include <Common/Memory/Containers/InlineVector.h>
#include <Common/Storage/AtomicIdentifierMask.h>
#include <Common/Threading/AtomicPtr.h>
#include <Common/EnumFlagOperators.h>
#include <Engine/Asset/AssetType.h>

#include <Renderer/Devices/LogicalDeviceIdentifier.h>
#include <Renderer/Constants.h>
#include <Renderer/Assets/StaticMesh/ProceduralMeshCache.h>
#include <Renderer/Assets/StaticMesh/StaticMeshFlags.h>
#include <Renderer/Buffers/StorageBuffer.h>

#include <Renderer/Descriptors/DescriptorSetLayout.h>
#include <Renderer/Descriptors/DescriptorSet.h>

namespace ngine
{
	struct Engine;

	namespace IO
	{
		struct LoadingThreadOnly;
		struct LoadingThreadJob;
	}

	namespace Threading
	{
		struct Job;
		struct JobBatch;
		struct StageBase;
		struct EngineJobRunnerThread;
	}
}

namespace ngine::Rendering
{
	struct Renderer;
	struct StaticMesh;
	struct StaticObject;
	struct RenderMesh;
	struct RenderMeshView;
	struct LogicalDevice;
	struct CommandBuffer;

	using StaticMeshGlobalLoadingCallback = Function<Threading::JobBatch(const StaticMeshIdentifier identifier), 8>;
	using StaticMeshPerLogicalDeviceLoadingCallback = void (*)(const StaticMeshIdentifier identifier, LogicalDevice& logicalDevice);

	struct StaticMeshInfo
	{
		StaticMeshInfo();
		StaticMeshInfo(StaticMeshGlobalLoadingCallback&& callback);
		StaticMeshInfo(StaticMeshGlobalLoadingCallback&& callback, UniquePtr<StaticMesh> pMesh);
		StaticMeshInfo(StaticMeshInfo&&) = default;
		~StaticMeshInfo();

		StaticMeshGlobalLoadingCallback m_globalLoadingCallback;
		UniquePtr<StaticMesh> m_pMesh;
	};

	enum class MeshLoadFlags : uint8
	{
		//! Assign a dummy texture if the requested one is not loaded yet (1, 1, 1, 0 if RGBA)
		LoadDummy = 1 << 0,
		Default = LoadDummy
	};
	ENUM_FLAG_OPERATORS(MeshLoadFlags);

	struct MeshCache final : public Asset::Type<StaticMeshIdentifier, StaticMeshInfo>
	{
		using BaseType = Type;

		MeshCache();
		virtual ~MeshCache();

		[[nodiscard]] PURE_LOCALS_AND_POINTERS Renderer& GetRenderer();
		[[nodiscard]] PURE_LOCALS_AND_POINTERS const Renderer& GetRenderer() const;

		[[nodiscard]] StaticMeshIdentifier FindOrRegisterAsset(const Asset::Guid guid);
		[[nodiscard]] StaticMeshIdentifier
		Create(StaticMeshGlobalLoadingCallback&& globalLoadingCallback, const EnumFlags<StaticMeshFlags> flags = {});
		void Remove(const StaticMeshIdentifier identifier);
		void DeregisterAsset(const IdentifierType identifier) = delete;

		[[nodiscard]] Optional<StaticMesh*> FindMesh(const StaticMeshIdentifier identifier) const
		{
			return BaseType::GetAssetData(identifier).m_pMesh;
		}
		[[nodiscard]] Optional<StaticMesh*> FindMesh(const StaticMeshIdentifier identifier)
		{
			return BaseType::GetAssetData(identifier).m_pMesh;
		}
		[[nodiscard]] StaticMesh& FindOrRegisterMesh(const Asset::Guid guid)
		{
			return *FindMesh(FindOrRegisterAsset(guid));
		}

		using MeshLoadEvent = ThreadSafe::Event<EventCallbackResult(void*, const StaticMeshIdentifier), 24, false>;
		using MeshLoadListenerData = MeshLoadEvent::ListenerData;
		using MeshLoadListenerIdentifier = MeshLoadEvent::ListenerIdentifier;

		[[nodiscard]] bool IsLoadingStaticMesh(const StaticMeshIdentifier identifier) const;
		[[nodiscard]] Threading::JobBatch TryLoadStaticMesh(const StaticMeshIdentifier identifier, MeshLoadListenerData&& listenerData);
		[[nodiscard]] Threading::JobBatch TryReloadStaticMesh(const StaticMeshIdentifier identifier);
		void OnMeshLoaded(const StaticMeshIdentifier identifier);
		void OnMeshLoadingFailed(const StaticMeshIdentifier identifier);

		enum class LoadedMeshFlags : uint8
		{
			IsDummy = 1 << 0
		};

		using RenderMeshLoadEvent = ThreadSafe::Event<EventCallbackResult(void*, const RenderMeshView&, const EnumFlags<LoadedMeshFlags>), 24>;
		using RenderMeshLoadListenerData = RenderMeshLoadEvent::ListenerData;
		using RenderMeshListenerIdentifier = RenderMeshLoadEvent::ListenerIdentifier;

		[[nodiscard]] Threading::JobBatch TryLoadRenderMesh(
			const LogicalDeviceIdentifier deviceIdentifier,
			const StaticMeshIdentifier identifier,
			RenderMeshLoadListenerData&& listenerData,
			const EnumFlags<MeshLoadFlags> flags = MeshLoadFlags::Default
		);
		bool RemoveRenderMeshListener(
			const LogicalDeviceIdentifier deviceIdentifier,
			const StaticMeshIdentifier identifier,
			const RenderMeshListenerIdentifier listenerIdentifier
		);

		[[nodiscard]] Optional<RenderMesh*>
		GetRenderMesh(const LogicalDeviceIdentifier deviceIdentifier, const StaticMeshIdentifier identifier) const
		{
			return m_perLogicalDeviceData[deviceIdentifier]->m_meshes[identifier];
		}
		[[nodiscard]] bool IsMeshLoaded(const StaticMeshIdentifier identifier) const;
		[[nodiscard]] bool IsRenderMeshLoaded(const LogicalDeviceIdentifier deviceIdentifier, const StaticMeshIdentifier identifier) const
		{
			return m_perLogicalDeviceData[deviceIdentifier]->m_meshes[identifier].IsValid();
		}
		void OnRenderMeshLoaded(
			const LogicalDeviceIdentifier deviceIdentifier,
			const StaticMeshIdentifier identifier,
			RenderMesh&& renderMesh,
			RenderMesh& previousMesh
		);

		[[nodiscard]] Threading::JobBatch ReloadMesh(const IdentifierType identifier, LogicalDevice& logicalDevice);
		[[nodiscard]] Threading::JobBatch ReloadMesh(const IdentifierType identifier);

		[[nodiscard]] StaticMeshIdentifier Clone(const StaticMeshIdentifier, const EnumFlags<StaticMeshFlags> flags = {});

		[[nodiscard]] const ProceduralMeshCache& GetProceduralMeshCache() const
		{
			return m_proceduralMeshCache;
		}
		[[nodiscard]] ProceduralMeshCache& GetProceduralMeshCache()
		{
			return m_proceduralMeshCache;
		}

		[[nodiscard]] DescriptorSetLayoutView GetMeshesDescriptorSetLayout(const LogicalDeviceIdentifier deviceIdentifier) const
		{
			const PerLogicalDeviceData& perDeviceData = *m_perLogicalDeviceData[deviceIdentifier];
			return perDeviceData.m_meshesDescriptorSetLayout;
		}
		[[nodiscard]] DescriptorSetView GetMeshesDescriptorSet(const LogicalDeviceIdentifier deviceIdentifier) const
		{
			const PerLogicalDeviceData& perDeviceData = *m_perLogicalDeviceData[deviceIdentifier];
			return perDeviceData.m_meshesDescriptorSet;
		}
	protected:
		void OnLogicalDeviceCreated(LogicalDevice& logicalDevice);

		[[nodiscard]] StaticMeshIdentifier RegisterAsset(const Asset::Guid guid);
		
		virtual void OnAssetModified(const Asset::Guid assetGuid, const IdentifierType identifier, const IO::PathView filePath) override;

		void CreateProceduralMeshes();

		friend struct LoadStaticMeshPerLogicalDeviceDataJob;

		struct MeshData
		{
			MeshLoadEvent m_onLoadedCallback;
		};
		MeshData& GetOrCreateMeshData(const StaticMeshIdentifier identifier);
		[[nodiscard]] Threading::JobBatch LoadRenderMeshData(const StaticMeshIdentifier identifier, LogicalDevice& logicalDevice);
		[[nodiscard]] Threading::JobBatch ReloadRenderMeshData(const StaticMeshIdentifier identifier, LogicalDevice& logicalDevice);
	protected:
		Threading::AtomicIdentifierMask<StaticMeshIdentifier> m_loadingMeshes;

		TIdentifierArray<Threading::Atomic<MeshData*>, StaticMeshIdentifier> m_meshData{Memory::Zeroed};

		ProceduralMeshCache m_proceduralMeshCache;

		struct PerLogicalDeviceData
		{
			TIdentifierArray<UniquePtr<RenderMesh>, StaticMeshIdentifier> m_meshes{Memory::Zeroed};
			UniquePtr<RenderMesh> m_pDummyMesh;

			Threading::EngineJobRunnerThread* m_pMeshesDescriptorPoolLoadingThread{nullptr};
			DescriptorSetLayout m_meshesDescriptorSetLayout;
			DescriptorSet m_meshesDescriptorSet;
			StorageBuffer m_meshesBuffer;

			Threading::AtomicIdentifierMask<StaticMeshIdentifier> m_loadingRenderMeshes;

			Threading::SharedMutex m_meshRequesterMutex;
			struct MeshRequesters
			{
				RenderMeshLoadEvent m_onLoadedCallback;
			};

			UnorderedMap<StaticMeshIdentifier, UniqueRef<MeshRequesters>, StaticMeshIdentifier::Hash> m_meshRequesterMap;
		};

		TIdentifierArray<UniquePtr<PerLogicalDeviceData>, LogicalDeviceIdentifier> m_perLogicalDeviceData;
	};

	ENUM_FLAG_OPERATORS(MeshCache::LoadedMeshFlags);
}
