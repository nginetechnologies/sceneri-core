#include "MeshCache.h"
#include "MeshAssetType.h"

#include <Engine/Asset/AssetType.inl>
#include <Engine/Threading/JobRunnerThread.h>

#include <Renderer/Renderer.h>
#include <Renderer/Assets/StaticMesh/StaticMesh.h>

#include <Common/Threading/Jobs/Job.h>
#include <Common/Threading/Jobs/JobBatch.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/Reflection/Registry.inl>
#include <Common/System/Query.h>
#include <Common/Serialization/Reader.h>
#include <Common/Memory/AddressOf.h>
#include <Common/Serialization/Deserialize.h>
#include <Common/IO/Log.h>

#include <3rdparty/jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <3rdparty/jolt/Physics/Collision/Shape/MeshShape.h>

namespace ngine::Physics
{
	MeshCache::MeshCache(Asset::Manager& assetManager)
	{
		RegisterAssetModifiedCallback(assetManager);
	}

	MeshCache::~MeshCache() = default;

	MeshIdentifier MeshCache::FindOrRegisterAsset(const Asset::Guid guid)
	{
		return BaseType::FindOrRegisterAsset(
			guid,
			[](const MeshIdentifier, const Asset::Guid)
			{
				return MeshInfo{};
			}
		);
	}

	MeshIdentifier MeshCache::RegisterAsset(const Asset::Guid guid)
	{
		const MeshIdentifier identifier = BaseType::RegisterAsset(
			guid,
			[](const MeshIdentifier, const Guid)
			{
				return MeshInfo{};
			}
		);
		return identifier;
	}

	MeshIdentifier MeshCache::FindOrRegisterAsset(const Rendering::StaticMeshIdentifier renderMeshIdentifier)
	{
		Threading::Atomic<typename MeshIdentifier::IndexType>& meshIdentifier = m_meshIdentifierIndexLookup[renderMeshIdentifier];
		typename MeshIdentifier::IndexType meshIdentifierIndex = meshIdentifier;
		if (meshIdentifierIndex != 0)
		{
			return MeshIdentifier::MakeFromIndex(meshIdentifierIndex);
		}

		const MeshIdentifier newMeshIdentifier = RegisterProceduralAsset(
			[](const MeshIdentifier, const Asset::Guid) mutable
			{
				return MeshInfo{};
			}
		);
		if (meshIdentifier.CompareExchangeStrong(meshIdentifierIndex, newMeshIdentifier.GetIndex()))
		{
			return newMeshIdentifier;
		}
		else
		{
			Assert(meshIdentifierIndex != 0);
			DeregisterAsset(newMeshIdentifier);
			return MeshIdentifier::MakeFromIndex(meshIdentifierIndex);
		}
	}

	// TODO: Shape::SaveBinaryState and Shape::sRestoreFromBinaryState
	// TOOD: Variant that reads .physmesh from disk (support both convex and mesh variants at the same time)

	Threading::JobBatch MeshCache::TryLoadOrCreateConvexMesh(
		const MeshIdentifier identifier, const Rendering::StaticMeshIdentifier renderMeshIdentifier, MeshLoadListenerData&& newListenerData
	)
	{
		MeshInfo& meshInfo = GetAssetData(identifier);
		meshInfo.m_onLoadConvexEvent.Emplace(Forward<MeshLoadListenerData>(newListenerData));

		if (meshInfo.m_pConvexShape.IsValid())
		{
			[[maybe_unused]] const bool wasExecuted =
				meshInfo.m_onLoadConvexEvent.ExecuteAndRemove(newListenerData.m_identifier, meshInfo.m_pConvexShape);
			Assert(wasExecuted);
			return {};
		}

		if (m_loadingConvexMeshes.Set(identifier))
		{
			if (!meshInfo.m_pConvexShape.IsValid())
			{
				Rendering::Renderer& renderer = System::Get<Rendering::Renderer>();
				Rendering::MeshCache& renderMeshCache = renderer.GetMeshCache();

				Threading::JobBatch intermediateStage{Threading::JobBatch::IntermediateStage};
				Threading::IntermediateStage& finishedLoadingStage = Threading::CreateIntermediateStage();
				finishedLoadingStage.AddSubsequentStage(intermediateStage.GetFinishedStage());

				Threading::JobBatch loadMeshJobBatch = renderMeshCache.TryLoadStaticMesh(
					renderMeshIdentifier,
					Rendering::MeshCache::MeshLoadListenerData{
						*this,
						[&finishedLoadingStage](MeshCache&, const Rendering::StaticMeshIdentifier)
						{
							finishedLoadingStage.SignalExecutionFinishedAndDestroying(*Threading::JobRunnerThread::GetCurrent());
							return EventCallbackResult::Remove;
						}
					}
				);

				Threading::Job& createFromRenderMeshJob = Threading::CreateCallback(
					[this, identifier, renderMeshIdentifier](Threading::JobRunnerThread&)
					{
						Rendering::Renderer& renderer = System::Get<Rendering::Renderer>();
						Rendering::MeshCache& renderMeshCache = renderer.GetMeshCache();
						Rendering::StaticMesh& renderMesh = *renderMeshCache.GetAssetData(renderMeshIdentifier).m_pMesh;

						const ArrayView<const Rendering::VertexPosition, Rendering::Index> meshVertexPositions = renderMesh.GetVertexPositions();
						FixedSizeVector<JPH::Vec3> position(Memory::ConstructWithSize, Memory::Uninitialized, meshVertexPositions.GetSize());

						// Copy vertex positions
						ArrayView<JPH::Vec3> targetVertexPositions = position.GetView();
						for (const Rendering::VertexPosition vertexPosition : meshVertexPositions)
						{
							targetVertexPositions[0] = {vertexPosition.x, vertexPosition.y, vertexPosition.z};
							targetVertexPositions++;
						}

						const float convexRadius = 0.05f;
						JPH::ConvexHullShapeSettings hullShapeSettings(position.GetData(), position.GetSize(), convexRadius);
						JPH::ShapeSettings::ShapeResult result = hullShapeSettings.Create();
						MeshInfo& meshInfo = GetAssetData(identifier);
						if (LIKELY(result.IsValid()))
						{
							Assert(!meshInfo.m_pConvexShape.IsValid());
							meshInfo.m_pConvexShape = result.Get();

							[[maybe_unused]] const bool cleared = m_loadingConvexMeshes.Clear(identifier);
							Assert(cleared);

							meshInfo.m_onLoadConvexEvent.ExecuteAndClear(result.Get());
						}
						else
						{
							[[maybe_unused]] const bool cleared = m_loadingConvexMeshes.Clear(identifier);
							Assert(cleared);

							meshInfo.m_onLoadConvexEvent.ExecuteAndClear(nullptr);
						}
					},
					Threading::JobPriority::CreateMeshCollider
				);
				intermediateStage.QueueAsNewFinishedStage(createFromRenderMeshJob);
				loadMeshJobBatch.QueueAsNewFinishedStage(intermediateStage);
				return loadMeshJobBatch;
			}
			else
			{
				[[maybe_unused]] const bool wasCleared = m_loadingConvexMeshes.Clear(identifier);
				Assert(wasCleared);
				[[maybe_unused]] const bool wasExecuted =
					meshInfo.m_onLoadConvexEvent.ExecuteAndRemove(newListenerData.m_identifier, meshInfo.m_pConvexShape);
				Assert(wasExecuted);
			}
		}
		return {};
	}

	Threading::JobBatch MeshCache::TryLoadOrCreateTriangleMesh(
		const MeshIdentifier identifier, const Rendering::StaticMeshIdentifier renderMeshIdentifier, MeshLoadListenerData&& newListenerData
	)
	{
		MeshInfo& meshInfo = GetAssetData(identifier);
		meshInfo.m_onLoadTriangleEvent.Emplace(Forward<MeshLoadListenerData>(newListenerData));

		if (meshInfo.m_pTriangleMeshShape.IsValid())
		{
			[[maybe_unused]] const bool wasExecuted =
				meshInfo.m_onLoadTriangleEvent.ExecuteAndRemove(newListenerData.m_identifier, meshInfo.m_pTriangleMeshShape);
			Assert(wasExecuted);
			return {};
		}

		if (m_loadingTriangleMeshes.Set(identifier))
		{
			if (!meshInfo.m_pTriangleMeshShape.IsValid())
			{
				// First attempt to request a the physmesh file.
				// If not present, compile at runtime.

				Rendering::Renderer& renderer = System::Get<Rendering::Renderer>();
				Rendering::MeshCache& renderMeshCache = renderer.GetMeshCache();

				Rendering::StaticMesh& renderMesh = *renderMeshCache.GetAssetData(renderMeshIdentifier).m_pMesh;

				Threading::JobBatch intermediateStage{Threading::JobBatch::IntermediateStage};
				Threading::IntermediateStage& finishedLoadingStage = Threading::CreateIntermediateStage();
				finishedLoadingStage.AddSubsequentStage(intermediateStage.GetFinishedStage());

				Threading::JobBatch loadMeshJobBatch = renderMeshCache.TryLoadStaticMesh(
					renderMeshIdentifier,
					Rendering::MeshCache::MeshLoadListenerData{
						*this,
						[&finishedLoadingStage](MeshCache&, const Rendering::StaticMeshIdentifier)
						{
							finishedLoadingStage.SignalExecutionFinishedAndDestroying(*Threading::JobRunnerThread::GetCurrent());
							return EventCallbackResult::Remove;
						}
					}
				);
				Threading::Job& createFromRenderMeshJob = Threading::CreateCallback(
					[this, identifier, &renderMesh](Threading::JobRunnerThread&)
					{
						const ArrayView<const Rendering::VertexPosition, Rendering::Index> vertexPositions = renderMesh.GetVertexPositions();
						ArrayView<const Rendering::Index, Rendering::Index> sourceIndices = renderMesh.GetIndices();

						JPH::MeshShapeSettings meshSettings;

						meshSettings.mTriangleVertices.resize(vertexPositions.GetSize());
						ArrayView<JPH::Float3, Rendering::Index> targetVertexPositions{
							meshSettings.mTriangleVertices.data(),
							(Rendering::Index)meshSettings.mTriangleVertices.size()
						};
						for (const Rendering::VertexPosition vertexPosition : vertexPositions)
						{
							targetVertexPositions[0] = {vertexPosition.x, vertexPosition.y, vertexPosition.z};
							targetVertexPositions++;
						}

						meshSettings.mIndexedTriangles.resize(sourceIndices.GetSize() / 3);
						ArrayView<JPH::IndexedTriangle, Rendering::Index> targetIndexedTriangles{
							meshSettings.mIndexedTriangles.data(),
							(Rendering::Index)meshSettings.mIndexedTriangles.size()
						};
						while (sourceIndices.HasElements())
						{
							const uint32 triangleMaterialIndex = 0;

							targetIndexedTriangles[0] = JPH::IndexedTriangle{sourceIndices[0], sourceIndices[1], sourceIndices[2], triangleMaterialIndex};

							targetIndexedTriangles++;
							sourceIndices += 3;
						}

						meshSettings.mMaterials.resize(1);
						meshSettings.mMaterials[0] = nullptr;

						JPH::ShapeSettings::ShapeResult result = meshSettings.Create();

						MeshInfo& meshInfo = GetAssetData(identifier);
						if (LIKELY(result.IsValid()))
						{
							Assert(!meshInfo.m_pTriangleMeshShape.IsValid());
							meshInfo.m_pTriangleMeshShape = result.Get();

							[[maybe_unused]] const bool cleared = m_loadingTriangleMeshes.Clear(identifier);
							Assert(cleared);

							meshInfo.m_onLoadTriangleEvent.ExecuteAndClear(result.Get());
						}
						else
						{
							[[maybe_unused]] const bool cleared = m_loadingTriangleMeshes.Clear(identifier);
							Assert(cleared);

							meshInfo.m_onLoadTriangleEvent.ExecuteAndClear(nullptr);
						}
					},
					Threading::JobPriority::CreateMeshCollider
				);
				intermediateStage.QueueAsNewFinishedStage(createFromRenderMeshJob);
				loadMeshJobBatch.QueueAsNewFinishedStage(intermediateStage);
				return loadMeshJobBatch;
			}
			else
			{
				[[maybe_unused]] const bool wasCleared = m_loadingTriangleMeshes.Clear(identifier);
				Assert(wasCleared);
				[[maybe_unused]] const bool wasExecuted =
					meshInfo.m_onLoadTriangleEvent.ExecuteAndRemove(newListenerData.m_identifier, meshInfo.m_pTriangleMeshShape);
				Assert(wasExecuted);
			}
		}
		return {};
	}

	Threading::JobBatch MeshCache::TryLoadMesh(const MeshIdentifier identifier, Asset::Manager& assetManager, LoadCallback&& callback)
	{
		// TODO: Read from disk

		UNUSED(callback);
		if (m_loadingConvexMeshes.Set(identifier))
		{
			const Guid assetGuid = GetAssetGuid(identifier);

			Threading::Job* pMeshLoadJob = assetManager.RequestAsyncLoadAssetMetadata(
				assetGuid,
				Threading::JobPriority::LoadMeshCollider,
				[assetGuid](const ConstByteView data)
				{
					if (UNLIKELY(!data.HasElements()))
					{
						LogWarning("Physical Mesh data was empty when loading asset {}!", assetGuid.ToString());
						return;
					}

					Serialization::Data meshMetadata(
						ConstStringView{reinterpret_cast<const char*>(data.GetData()), static_cast<uint32>(data.GetDataSize() / sizeof(char))}
					);
					if (UNLIKELY(!meshMetadata.IsValid()))
					{
						LogWarning("Physical Mesh data was invalid when loading asset {}!", assetGuid.ToString());
						return;
					}

					const Serialization::Reader meshReader(meshMetadata);

					// TODO: Load
				}
			);

			if (pMeshLoadJob != nullptr)
			{
				Threading::JobBatch jobBatch(Threading::JobBatch::IntermediateStage);
				jobBatch.QueueAfterStartStage(*pMeshLoadJob);
				return jobBatch;
			}
		}
		return {};
	}

#if DEVELOPMENT_BUILD
	void MeshCache::OnAssetModified(
		[[maybe_unused]] const Asset::Guid assetGuid,
		[[maybe_unused]] const IdentifierType identifier,
		[[maybe_unused]] const IO::PathView filePath
	)
	{
		// TODO: Implement
	}
#endif
}
