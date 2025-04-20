#include "BuildAccelerationStructureStage.h"

#include <Renderer/Renderer.h>
#include <Renderer/Scene/SceneView.h>
#include <Renderer/Assets/StaticMesh/RenderMesh.h>
#include <Renderer/Assets/Material/RuntimeMaterialInstance.h>
#include <Renderer/Assets/Material/RuntimeMaterial.h>
#include <Renderer/Assets/StaticMesh/ForwardDeclarations/VertexPosition.h>
#include <Renderer/Commands/BlitCommandEncoder.h>
#include <Renderer/Commands/AccelerationStructureCommandEncoder.h>
#include <Renderer/Buffers/StagingBuffer.h>
#include <Renderer/Buffers/DataToBufferBatch.h>
#include <Renderer/Stages/PerFrameStagingBuffer.h>
#include <Renderer/Wrappers/AccelerationStructureInstance.h>

#include <Engine/Scene/Scene.h>
#include <Engine/Entity/Data/WorldTransform.h>
#include <Engine/Entity/ComponentTypeSceneData.h>
#include <Engine/Entity/Data/RenderItem/StaticMeshIdentifier.h>
#include <Engine/Entity/Data/RenderItem/MaterialInstanceIdentifier.h>
#include <Engine/Entity/Data/Flags.h>

#include <Common/System/Query.h>

namespace ngine::Rendering
{
	// TODO: Support stages outside of the scene view context
	// That way we can share acceleration structures between views

#if RENDERER_SUPPORTS_RAYTRACING
	inline static constexpr uint32 MaximumInstanceCount = 65535;
#endif

	BuildAccelerationStructureStage::BuildAccelerationStructureStage(SceneView& sceneView)
		: RenderItemStage(sceneView.GetLogicalDevice(), Threading::JobPriority::Draw)
		, m_sceneView(sceneView)
#if RENDERER_SUPPORTS_RAYTRACING
		, m_instanceBuffer(InstanceBuffer{
				sceneView.GetLogicalDevice(),
				MaximumInstanceCount,
				sizeof(InstanceAccelerationStructure::Instance),
				Buffer::UsageFlags::ShaderDeviceAddress | Buffer::UsageFlags::AccelerationStructureBuildInputReadOnly
			})
		, m_instanceCountBuffer(
				sceneView.GetLogicalDevice(),
				sceneView.GetLogicalDevice().GetPhysicalDevice(),
				sceneView.GetLogicalDevice().GetDeviceMemoryPool(),
				sizeof(uint32),
				Buffer::UsageFlags::TransferDestination | Buffer::UsageFlags::ShaderDeviceAddress,
				MemoryFlags::DeviceLocal | MemoryFlags::AllocateDeviceAddress
			)
#endif
	{
		Rendering::Renderer& renderer = System::Get<Rendering::Renderer>();
		const SceneRenderStageIdentifier stageIdentifier = renderer.GetStageCache().FindOrRegisterAsset(Guid);
		m_sceneView.RegisterRenderItemStage(stageIdentifier, *this);
		m_sceneView.SetStageDependentOnCameraProperties(stageIdentifier);
	}

	BuildAccelerationStructureStage::~BuildAccelerationStructureStage()
	{
#if RENDERER_SUPPORTS_RAYTRACING
		m_instanceBuffer.Destroy(m_sceneView.GetLogicalDevice());
		m_instanceCountBuffer.Destroy(m_sceneView.GetLogicalDevice(), m_sceneView.GetLogicalDevice().GetDeviceMemoryPool());
#endif

		Rendering::Renderer& renderer = System::Get<Rendering::Renderer>();
		m_sceneView.DeregisterRenderItemStage(renderer.GetStageCache().FindIdentifier(Guid));
	}

	void BuildAccelerationStructureStage::OnRenderItemsBecomeVisible(
		const Entity::RenderItemMask& renderItems,
		[[maybe_unused]] const Rendering::CommandEncoderView graphicsCommandEncoder,
		PerFrameStagingBuffer& perFrameStagingBuffer
	)
	{
#if RENDERER_SUPPORTS_RAYTRACING
		Scene& scene = m_sceneView.GetScene();
		const typename Entity::RenderItemIdentifier::IndexType maximumUsedRenderItemCount = scene.GetMaximumUsedRenderItemCount();

		Entity::SceneRegistry& sceneRegistry = scene.GetEntitySceneRegistry();

		Rendering::Renderer& renderer = System::Get<Rendering::Renderer>();
		Rendering::MeshCache& meshCache = renderer.GetMeshCache();

		const Rendering::LogicalDevice& logicalDevice = m_sceneView.GetLogicalDevice();
		const LogicalDeviceIdentifier logicalDeviceIdentifier = logicalDevice.GetIdentifier();

		Entity::ComponentTypeSceneData<Entity::Data::RenderItem::StaticMeshIdentifier>& staticMeshIdentifierSceneData =
			sceneRegistry.GetCachedSceneData<Entity::Data::RenderItem::StaticMeshIdentifier>();

		Entity::RenderItemMask addedRenderItems;

		for (const uint32 renderItemIndex : renderItems.GetSetBitsIterator(0, maximumUsedRenderItemCount))
		{
			const Entity::RenderItemIdentifier renderItemIdentifier = Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex);
			const Entity::ComponentIdentifier componentIdentifier = m_sceneView.GetVisibleRenderItemComponentIdentifier(renderItemIdentifier);
			Assert(renderItemIdentifier.IsValid());
			if (UNLIKELY_ERROR(renderItemIdentifier.IsInvalid()))
			{
				continue;
			}

			const Optional<Entity::Data::RenderItem::StaticMeshIdentifier*> pStaticMeshIdentifierComponent =
				staticMeshIdentifierSceneData.GetComponentImplementation(componentIdentifier);
			Assert(pStaticMeshIdentifierComponent.IsValid());
			if (pStaticMeshIdentifierComponent.IsInvalid())
			{
				continue;
			}

			if (!m_processedRenderItems.IsSet(renderItemIdentifier))
			{
				const InstanceBuffer::InstanceIndexType instanceIndex = m_instanceBuffer.AddInstance();
				Assert(instanceIndex < m_instanceBuffer.GetCapacity());
				if (UNLIKELY(instanceIndex == m_instanceBuffer.GetCapacity()))
				{
					continue;
				}

				m_instanceIndices[renderItemIdentifier] = instanceIndex;
				m_instanceRenderItems[instanceIndex] = renderItemIdentifier.GetFirstValidIndex();

				addedRenderItems.Set(renderItemIdentifier);
				m_processedRenderItems.Set(renderItemIdentifier);
			}

			const Rendering::StaticMeshIdentifier meshIdentifier = *pStaticMeshIdentifierComponent;
			if (!m_builtMeshes.IsSet(meshIdentifier))
			{
				m_meshAccelerationStructureData[meshIdentifier].m_pendingData.m_pendingRenderItems.Set(renderItemIdentifier);

				if (!m_loadedMeshes.IsSet(meshIdentifier))
				{
					m_loadedMeshes.Set(meshIdentifier);

					Threading::JobBatch jobBatch = meshCache.TryLoadRenderMesh(
						logicalDeviceIdentifier,
						meshIdentifier,
						Rendering::MeshCache::RenderMeshLoadListenerData{
							*this,
							[meshIdentifier,
					     &logicalDevice](BuildAccelerationStructureStage& buildAccelerationStructureStage, RenderMeshView newRenderMesh, const EnumFlags<MeshCache::LoadedMeshFlags>)
							{
								if (buildAccelerationStructureStage.m_buildingMeshes.Set(meshIdentifier))
								{
									Threading::UniqueLock lock(buildAccelerationStructureStage.m_queuedGeometryBuildsMutex);
									QueuedGeometryBuild& geometryBuild = buildAccelerationStructureStage.m_queuedGeometryBuilds.EmplaceBack();
									geometryBuild.m_meshIdentifier = meshIdentifier;

									geometryBuild.m_queuedMeshAccelerationStructureGeometry.EmplaceBack(
										logicalDevice,
										Format::R32G32B32_SFLOAT,
										newRenderMesh.GetVertexBuffer(),
										sizeof(Rendering::VertexPosition),
										newRenderMesh.GetVertexCount(),
										newRenderMesh.GetIndexCount(),
										newRenderMesh.GetIndexBuffer()
									);

									geometryBuild.m_queuedMeshTriangleCounts.EmplaceBack(newRenderMesh.GetTriangleCount());

									geometryBuild.m_queuedMeshBuildRanges.EmplaceBack(
										PrimitiveAccelerationStructureBuildRangeInfo{newRenderMesh.GetTriangleCount(), 0, 0, 0}
									);
									return EventCallbackResult::Remove;
								}
								return EventCallbackResult::Keep;
							}
						},
						MeshLoadFlags::Default & ~MeshLoadFlags::LoadDummy
					);
					if (jobBatch.IsValid())
					{
						Threading::JobRunnerThread::GetCurrent()->Queue(jobBatch);
					}
				}
			}
		}

		if (addedRenderItems.AreAnySet())
		{
			OnVisibleRenderItemTransformsChanged(addedRenderItems, graphicsCommandEncoder, perFrameStagingBuffer);
		}

		m_shouldRebuildInstances = true;
#else
		UNUSED(renderItems);
		UNUSED(graphicsCommandEncoder);
		UNUSED(perFrameStagingBuffer);
#endif
	}

	void BuildAccelerationStructureStage::OnVisibleRenderItemsReset(
		const Entity::RenderItemMask& renderItems,
		const Rendering::CommandEncoderView graphicsCommandEncoder,
		PerFrameStagingBuffer& perFrameStagingBuffer

	)
	{
		OnRenderItemsBecomeHidden(renderItems, *m_sceneView.GetSceneChecked(), graphicsCommandEncoder, perFrameStagingBuffer);
		OnRenderItemsBecomeVisible(renderItems, graphicsCommandEncoder, perFrameStagingBuffer);
	}

	void BuildAccelerationStructureStage::OnRenderItemsBecomeHidden(
		const Entity::RenderItemMask& renderItems,
		[[maybe_unused]] SceneBase& scene,
		[[maybe_unused]] const Rendering::CommandEncoderView graphicsCommandEncoder,
		[[maybe_unused]] PerFrameStagingBuffer& perFrameStagingBuffer
	)
	{
#if RENDERER_SUPPORTS_RAYTRACING
		const typename Entity::RenderItemIdentifier::IndexType maximumUsedRenderItemCount = scene.GetMaximumUsedRenderItemCount();
		Entity::SceneRegistry& sceneRegistry = scene.GetEntitySceneRegistry();
		Entity::ComponentTypeSceneData<Entity::Data::RenderItem::StaticMeshIdentifier>& staticMeshIdentifierSceneData =
			sceneRegistry.GetCachedSceneData<Entity::Data::RenderItem::StaticMeshIdentifier>();

		// Temp while hacked into shadows stage
		Entity::RenderItemMask validRenderItems;
		for (const uint32 renderItemIndex : renderItems.GetSetBitsIterator(0, maximumUsedRenderItemCount))
		{
			const Entity::RenderItemIdentifier renderItemIdentifier = Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex);
			const Entity::ComponentIdentifier componentIdentifier = m_sceneView.GetVisibleRenderItemComponentIdentifier(renderItemIdentifier);
			Assert(renderItemIdentifier.IsValid());
			if (UNLIKELY_ERROR(renderItemIdentifier.IsInvalid()))
			{
				continue;
			}

			if (const Optional<Entity::Data::RenderItem::StaticMeshIdentifier*> pStaticMeshIdentifierComponent = staticMeshIdentifierSceneData.GetComponentImplementation(componentIdentifier))
			{
				validRenderItems.Set(renderItemIdentifier);
				continue;
			}
		}

		for (const uint32 renderItemIndex : validRenderItems.GetSetBitsIterator(0, maximumUsedRenderItemCount))
		{
			const Entity::RenderItemIdentifier renderItemIdentifier = Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex);
			const InstanceBuffer::InstanceIndexType renderItemInstanceIndex = m_instanceIndices[renderItemIdentifier];

			switch (m_instanceBuffer.RemoveInstance(renderItemInstanceIndex))
			{
				case InstanceBuffer::RemovalResult::RemovedLastRemainingInstance:
				case InstanceBuffer::RemovalResult::RemovedFrontInstance:
				case InstanceBuffer::RemovalResult::RemovedBackInstance:
					break;
				case InstanceBuffer::RemovalResult::RemovedMiddleInstance:
				{
					const InstanceBuffer::InstanceIndexType lastInstanceIndex = m_instanceBuffer.GetFirstInstanceIndex() +
					                                                            m_instanceBuffer.GetInstanceCount();
					// Move the last instance to the now unused index
					const decltype(m_instanceIndices)::DynamicView instanceIndices = scene.GetValidRenderItemView(m_instanceIndices.GetView());
					for (InstanceBuffer::InstanceIndexType& instanceIndex : instanceIndices)
					{
						if (instanceIndex == lastInstanceIndex)
						{
							instanceIndex = renderItemInstanceIndex;

							m_instanceBuffer.MoveElement(
								lastInstanceIndex,
								instanceIndex,
								graphicsCommandEncoder,
								sizeof(InstanceAccelerationStructure::Instance),
								perFrameStagingBuffer
							);

							return;
						}
					}

					ExpectUnreachable();
				}
			}
		}

		m_shouldRebuildInstances = true;
#else
		UNUSED(renderItems);
#endif
	}

	void BuildAccelerationStructureStage::OnVisibleRenderItemTransformsChanged(
		const Entity::RenderItemMask& renderItems,
		[[maybe_unused]] const Rendering::CommandEncoderView graphicsCommandEncoder,
		PerFrameStagingBuffer& perFrameStagingBuffer
	)
	{
#if RENDERER_SUPPORTS_RAYTRACING
		Scene& scene = m_sceneView.GetScene();
		Entity::SceneRegistry& sceneRegistry = scene.GetEntitySceneRegistry();
		Entity::ComponentTypeSceneData<Entity::Data::WorldTransform>& worldTransformSceneData =
			sceneRegistry.GetCachedSceneData<Entity::Data::WorldTransform>();
		Entity::ComponentTypeSceneData<Entity::Data::RenderItem::StaticMeshIdentifier>& staticMeshIdentifierSceneData =
			sceneRegistry.GetCachedSceneData<Entity::Data::RenderItem::StaticMeshIdentifier>();
		Entity::ComponentTypeSceneData<Entity::Data::RenderItem::MaterialInstanceIdentifier>& materialInstanceIdentifierSceneData =
			sceneRegistry.GetCachedSceneData<Entity::Data::RenderItem::MaterialInstanceIdentifier>();

		const typename Entity::RenderItemIdentifier::IndexType maximumUsedRenderItemCount = scene.GetMaximumUsedRenderItemCount();

		// Temp while hacked into shadows stage
		Entity::RenderItemMask validRenderItems;
		for (const uint32 renderItemIndex : renderItems.GetSetBitsIterator(0, maximumUsedRenderItemCount))
		{
			const Entity::RenderItemIdentifier renderItemIdentifier = Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex);
			const Optional<Entity::HierarchyComponentBase*> pVisibleComponent =
				m_sceneView.GetVisibleRenderItemComponent(Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex));
			Assert(pVisibleComponent.IsValid());
			if (UNLIKELY_ERROR(pVisibleComponent.IsInvalid()))
			{
				continue;
			}

			if (pVisibleComponent->IsStaticMesh(sceneRegistry))
			{
				validRenderItems.Set(renderItemIdentifier);
			}
		}

		InstanceBuffer::InstanceIndexType minimumInstanceIndex = Math::NumericLimits<InstanceBuffer::InstanceIndexType>::Max;
		InstanceBuffer::InstanceIndexType maximumInstanceIndex = Math::NumericLimits<InstanceBuffer::InstanceIndexType>::Min;
		for (const uint32 renderItemIndex : validRenderItems.GetSetBitsIterator(0, maximumUsedRenderItemCount))
		{
			const Entity::RenderItemIdentifier renderItemIdentifier = Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex);
			const InstanceBuffer::InstanceIndexType renderItemInstanceIndex = m_instanceIndices[renderItemIdentifier];
			minimumInstanceIndex = Math::Min(renderItemInstanceIndex, minimumInstanceIndex);
			maximumInstanceIndex = Math::Max(renderItemInstanceIndex, maximumInstanceIndex);
		}

		const InstanceBuffer::InstanceIndexType totalAffectedItemCount = maximumInstanceIndex - minimumInstanceIndex + 1;

		m_instanceBuffer.StartBatchUpload(
			minimumInstanceIndex,
			totalAffectedItemCount,
			graphicsCommandEncoder,
			sizeof(InstanceAccelerationStructure::Instance)
		);

		const Rendering::BufferView instanceBuffer = m_instanceBuffer.GetBuffer();

		Bitset<MaximumInstanceCount> changedInstances;
		for (const uint32 renderItemIndex : validRenderItems.GetSetBitsIterator(0, maximumUsedRenderItemCount))
		{
			const Entity::RenderItemIdentifier renderItemIdentifier = Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex);
			const InstanceBuffer::InstanceIndexType renderItemInstanceIndex = m_instanceIndices[renderItemIdentifier];
			changedInstances.Set((uint16)renderItemInstanceIndex);
		}

		Rendering::Renderer& renderer = System::Get<Rendering::Renderer>();
		Rendering::MaterialCache& materialCache = renderer.GetMaterialCache();
		Rendering::MaterialInstanceCache& materialInstanceCache = materialCache.GetInstanceCache();

		Rendering::LogicalDevice& logicalDevice = m_logicalDevice;
		const CommandQueueView graphicsCommandQueue = logicalDevice.GetCommandQueue(QueueFamily::Graphics);
		changedInstances.IterateSetBitRanges(
			[&perFrameStagingBuffer,
		   &staticMeshIdentifierSceneData,
		   &materialInstanceIdentifierSceneData,
		   instanceBuffer,
		   instanceRenderItems = m_instanceRenderItems.GetView(),
		   graphicsCommandQueue,
		   graphicsCommandEncoder,
		   &sceneView = m_sceneView,
		   &logicalDevice,
		   &materialInstanceCache,
		   &materialCache,
		   &worldTransformSceneData,
		   meshAccelerationStructureData = m_meshAccelerationStructureData.GetView()](const Math::Range<uint16> setBits) mutable
			{
				const uint32 firstInstanceIndex = setBits.GetMinimum();
				const uint32 instanceCount = setBits.GetSize();
				PerFrameStagingBuffer::BatchCopyContext batchCopyContext = perFrameStagingBuffer.BeginBatchCopyToBuffer(
					sizeof(InstanceAccelerationStructure::Instance) * instanceCount,
					instanceBuffer,
					firstInstanceIndex * sizeof(InstanceAccelerationStructure::Instance)
				);

				uint32 localIndex = 0;
				for (const uint32 instanceIndex : setBits)
				{
					// const uint32 index = instanceIndex - minimumInstanceIndex;

					const Entity::RenderItemIdentifier::IndexType renderItemIndex = instanceRenderItems[instanceIndex];
					const Entity::RenderItemIdentifier renderItemIdentifier = Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex);
					const Entity::ComponentIdentifier componentIdentifier = sceneView.GetVisibleRenderItemComponentIdentifier(renderItemIdentifier);
					Assert(componentIdentifier.IsValid());
					if (UNLIKELY_ERROR(componentIdentifier.IsInvalid()))
					{
						continue;
					}

					const Rendering::MaterialInstanceIdentifier materialInstanceIdentifier =
						materialInstanceIdentifierSceneData.GetComponentImplementationUnchecked(componentIdentifier);
					const Rendering::StaticMeshIdentifier meshIdentifier =
						staticMeshIdentifierSceneData.GetComponentImplementationUnchecked(componentIdentifier);

					EnumFlags<InstanceAccelerationStructure::Instance::Flags> instanceFlags{};

					const Optional<const Rendering::RuntimeMaterialInstance*> pMaterialInstance =
						materialInstanceCache.GetMaterialInstance(materialInstanceIdentifier);
					if (pMaterialInstance.IsValid())
					{
						const Rendering::MaterialIdentifier materialIdentifier = pMaterialInstance->GetMaterialIdentifier();
						if (materialIdentifier.IsValid())
						{
							const Optional<const Rendering::RuntimeMaterial*> pMaterial =
								materialCache.GetAssetData(materialIdentifier).m_pMaterial.Get();
							if (pMaterial.IsValid())
							{
								if (const Optional<const Rendering::MaterialAsset*> pMaterialAsset = *pMaterial->GetAsset())
								{
									instanceFlags |=
										(InstanceAccelerationStructure::Instance::Flags::DisableTriangleFaceCulling * pMaterialAsset->m_twoSided);
								}
							}
						}
					}

					const Math::WorldTransform worldTransform = worldTransformSceneData.GetComponentImplementationUnchecked(componentIdentifier);

					const uint8 visibilityMask = Math::NumericLimits<uint8>::Max;

					Math::Matrix3x4f matrix(worldTransform);
					matrix.Orthonormalize();

					const InstanceAccelerationStructure::Instance instance{
						matrix,
						renderItemIdentifier.GetFirstValidIndex(),
						visibilityMask,
						instanceFlags,
						meshAccelerationStructureData[meshIdentifier].m_resourceIdentifier
					};

					perFrameStagingBuffer.BatchCopyToBuffer(
						logicalDevice,
						batchCopyContext,
						graphicsCommandQueue,
						ConstByteView::Make(instance),
						localIndex * sizeof(InstanceAccelerationStructure::Instance)
					);
					localIndex++;
				}

				perFrameStagingBuffer.EndBatchCopyToBuffer(batchCopyContext, graphicsCommandEncoder);
			}
		);

		m_instanceBuffer.EndBatchUpload(
			minimumInstanceIndex,
			totalAffectedItemCount,
			graphicsCommandEncoder,
			sizeof(InstanceAccelerationStructure::Instance)
		);

		m_shouldRebuildInstances = true;
#else
		UNUSED(renderItems);
		UNUSED(perFrameStagingBuffer);
#endif
	}

	void BuildAccelerationStructureStage::
		OnActiveCameraPropertiesChanged([[maybe_unused]] const Rendering::CommandEncoderView graphicsCommandEncoder, PerFrameStagingBuffer&)
	{
	}

	Threading::JobBatch BuildAccelerationStructureStage::LoadRenderItemsResources(const Entity::RenderItemMask& renderItems)
	{
#if RENDERER_SUPPORTS_RAYTRACING
		Threading::JobBatch jobBatch;

		Scene& scene = m_sceneView.GetScene();
		const typename Entity::RenderItemIdentifier::IndexType maximumUsedRenderItemCount = scene.GetMaximumUsedRenderItemCount();

		Entity::SceneRegistry& sceneRegistry = scene.GetEntitySceneRegistry();

		Rendering::Renderer& renderer = System::Get<Rendering::Renderer>();
		Rendering::MeshCache& meshCache = renderer.GetMeshCache();

		const Rendering::LogicalDevice& logicalDevice = m_sceneView.GetLogicalDevice();
		const LogicalDeviceIdentifier logicalDeviceIdentifier = logicalDevice.GetIdentifier();

		Entity::ComponentTypeSceneData<Entity::Data::RenderItem::StaticMeshIdentifier>& staticMeshIdentifierSceneData =
			sceneRegistry.GetCachedSceneData<Entity::Data::RenderItem::StaticMeshIdentifier>();

		for (const uint32 renderItemIndex : renderItems.GetSetBitsIterator(0, maximumUsedRenderItemCount))
		{
			const Entity::RenderItemIdentifier renderItemIdentifier = Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex);
			const Entity::ComponentIdentifier componentIdentifier = m_sceneView.GetVisibleRenderItemComponentIdentifier(renderItemIdentifier);
			Assert(renderItemIdentifier.IsValid());
			if (UNLIKELY_ERROR(renderItemIdentifier.IsInvalid()))
			{
				continue;
			}

			const Optional<Entity::Data::RenderItem::StaticMeshIdentifier*> pStaticMeshIdentifierComponent =
				staticMeshIdentifierSceneData.GetComponentImplementation(componentIdentifier);
			Assert(pStaticMeshIdentifierComponent.IsValid());
			if (pStaticMeshIdentifierComponent.IsInvalid())
			{
				continue;
			}

			const Rendering::StaticMeshIdentifier meshIdentifier = *pStaticMeshIdentifierComponent;
			if (!m_builtMeshes.IsSet(meshIdentifier))
			{
				// m_meshAccelerationStructureData[meshIdentifier].m_pendingData.m_pendingRenderItems.Set(renderItemIdentifier);

				if (!m_loadedMeshes.IsSet(meshIdentifier))
				{
					m_loadedMeshes.Set(meshIdentifier);

					Threading::JobBatch meshJobBatch = meshCache.TryLoadRenderMesh(
						logicalDeviceIdentifier,
						meshIdentifier,
						Rendering::MeshCache::RenderMeshLoadListenerData{
							*this,
							[meshIdentifier,
					     &logicalDevice](BuildAccelerationStructureStage& buildAccelerationStructureStage, RenderMeshView newRenderMesh, const EnumFlags<MeshCache::LoadedMeshFlags>)
							{
								if (buildAccelerationStructureStage.m_buildingMeshes.Set(meshIdentifier))
								{
									Threading::UniqueLock lock(buildAccelerationStructureStage.m_queuedGeometryBuildsMutex);
									QueuedGeometryBuild& geometryBuild = buildAccelerationStructureStage.m_queuedGeometryBuilds.EmplaceBack();
									geometryBuild.m_meshIdentifier = meshIdentifier;

									geometryBuild.m_queuedMeshAccelerationStructureGeometry.EmplaceBack(
										logicalDevice,
										Format::R32G32B32_SFLOAT,
										newRenderMesh.GetVertexBuffer(),
										sizeof(Rendering::VertexPosition),
										newRenderMesh.GetVertexCount(),
										newRenderMesh.GetIndexCount(),
										newRenderMesh.GetIndexBuffer()
									);

									geometryBuild.m_queuedMeshTriangleCounts.EmplaceBack(newRenderMesh.GetTriangleCount());

									geometryBuild.m_queuedMeshBuildRanges.EmplaceBack(
										PrimitiveAccelerationStructureBuildRangeInfo{newRenderMesh.GetTriangleCount(), 0, 0, 0}
									);
									return EventCallbackResult::Remove;
								}
								return EventCallbackResult::Keep;
							}
						},
						MeshLoadFlags::Default & ~MeshLoadFlags::LoadDummy
					);
					if (jobBatch.IsValid())
					{
						jobBatch.QueueAfterStartStage(meshJobBatch);
					}
				}
			}
		}

		m_shouldRebuildInstances = true;
		return jobBatch;
#else
		UNUSED(renderItems);
		return {};
#endif
	}

	void BuildAccelerationStructureStage::OnSceneUnloaded()
	{
		// TODO: Clear instances?
	}

	bool BuildAccelerationStructureStage::ShouldRecordCommands() const
	{
#if RENDERER_SUPPORTS_RAYTRACING
		return m_queuedGeometryBuilds.HasElements() || m_shouldRebuildInstances;
#else
		return false;
#endif
	}

	void BuildAccelerationStructureStage::RecordCommands(const CommandEncoderView commandEncoder)
	{
#if RENDERER_SUPPORTS_RAYTRACING

		{
			Threading::UniqueLock lock(m_queuedGeometryBuildsMutex);
			if (m_queuedGeometryBuilds.HasElements())
			{
				BuildGeometry(commandEncoder);
			}
		}

		if (m_shouldRebuildInstances)
		{
			m_shouldRebuildInstances = false;
			BuildInstances(commandEncoder);
		}

		{
			const BlitCommandEncoder blitCommandEncoder = commandEncoder.BeginBlit();

			Optional<StagingBuffer> stagingBuffer;
			const uint32 instanceCount = m_instanceBuffer.GetInstanceCount();
			LogicalDevice& logicalDevice = m_sceneView.GetLogicalDevice();
			blitCommandEncoder.RecordCopyDataToBuffer(
				logicalDevice,
				QueueFamily::Graphics,
				Array{DataToBufferBatch{m_instanceCountBuffer, Array{DataToBuffer{0, ConstByteView::Make(instanceCount)}}}},
				stagingBuffer
			);
			if (stagingBuffer.IsValid())
			{
				Threading::EngineJobRunnerThread::GetCurrent()->GetRenderData().DestroyBuffer(logicalDevice.GetIdentifier(), Move(*stagingBuffer));
			}
		}
#else
		UNUSED(commandEncoder);
#endif
	}

	// TODO: Receive mesh change callbacks and update the acceleration structures
	// TODO: Don't submit geometry acceleration structure commands in the context of a frame, make it fully async. Ideally on async compute

	void BuildAccelerationStructureStage::BuildGeometry(const Rendering::CommandEncoderView graphicsCommandEncoder)
	{
#if RENDERER_SUPPORTS_RAYTRACING
		Assert(m_queuedGeometryBuilds.HasElements());

		Scene& scene = m_sceneView.GetScene();
		const typename Entity::RenderItemIdentifier::IndexType maximumUsedRenderItemCount = scene.GetMaximumUsedRenderItemCount();

		Entity::SceneRegistry& sceneRegistry = scene.GetEntitySceneRegistry();
		Entity::ComponentTypeSceneData<Entity::Data::WorldTransform>& worldTransformSceneData =
			sceneRegistry.GetCachedSceneData<Entity::Data::WorldTransform>();
		Entity::ComponentTypeSceneData<Entity::Data::RenderItem::StaticMeshIdentifier>& staticMeshIdentifierSceneData =
			sceneRegistry.GetCachedSceneData<Entity::Data::RenderItem::StaticMeshIdentifier>();
		Entity::ComponentTypeSceneData<Entity::Data::RenderItem::MaterialInstanceIdentifier>& materialInstanceIdentifierSceneData =
			sceneRegistry.GetCachedSceneData<Entity::Data::RenderItem::MaterialInstanceIdentifier>();

		Rendering::LogicalDevice& logicalDevice = m_sceneView.GetLogicalDevice();

		Rendering::Renderer& renderer = System::Get<Rendering::Renderer>();
		Rendering::MaterialCache& materialCache = renderer.GetMaterialCache();
		Rendering::MaterialInstanceCache& materialInstanceCache = materialCache.GetInstanceCache();

		Threading::EngineJobRunnerThread& thread = *Threading::EngineJobRunnerThread::GetCurrent();

		for (const QueuedGeometryBuild& queuedGeometryBuild : m_queuedGeometryBuilds)
		{
			const StaticMeshIdentifier meshIdentifier = queuedGeometryBuild.m_meshIdentifier;
			Assert(!m_builtMeshes.IsSet(meshIdentifier));
			Assert(m_buildingMeshes.IsSet(meshIdentifier));

			const ArrayView<const PrimitiveAccelerationStructure::TriangleGeometryDescriptor> queuedGeometry =
				queuedGeometryBuild.m_queuedMeshAccelerationStructureGeometry.GetView();

			PrimitiveAccelerationStructureData& __restrict meshAccelerationStructureData = m_meshAccelerationStructureData[meshIdentifier];
			PrimitiveAccelerationStructure& __restrict meshAccelerationStructure = m_meshAccelerationStructures[meshIdentifier];
			PendingAccelerationStructureData& __restrict pendingMeshAccelerationStructureData = meshAccelerationStructureData.m_pendingData;

			if (meshAccelerationStructureData.m_buffer.IsValid())
			{
				thread.GetRenderData().DestroyBuffer(logicalDevice.GetIdentifier(), Move(meshAccelerationStructureData.m_buffer));
				thread.GetRenderData().DestroyBuffer(logicalDevice.GetIdentifier(), Move(pendingMeshAccelerationStructureData.m_scratchBuffer));
			}

			PrimitiveAccelerationStructureDescriptor primitiveDescriptor{queuedGeometry};

			const PrimitiveAccelerationStructure::BuildSizesInfo accelerationStructureBuildSizes =
				primitiveDescriptor.CalculateBuildSizes(logicalDevice, queuedGeometryBuild.m_queuedMeshTriangleCounts);

			Assert(!meshAccelerationStructureData.m_buffer.IsValid());
			meshAccelerationStructureData.m_buffer = Buffer(
				logicalDevice,
				logicalDevice.GetPhysicalDevice(),
				logicalDevice.GetDeviceMemoryPool(),
				accelerationStructureBuildSizes.accelerationStructureSize,
				Buffer::UsageFlags::AccelerationStructureStorage | Buffer::UsageFlags::ShaderDeviceAddress,
				MemoryFlags::DeviceLocal | MemoryFlags::AllocateDeviceAddress
			);

			PrimitiveAccelerationStructure accelerationStructure{
				logicalDevice,
				meshAccelerationStructureData.m_buffer,
				meshAccelerationStructureData.m_buffer.GetBoundMemory(),
				meshAccelerationStructureData.m_buffer.GetOffset(),
				accelerationStructureBuildSizes.accelerationStructureSize
			};
			Assert(accelerationStructure.IsValid());

			meshAccelerationStructureData.m_resourceIdentifier = accelerationStructure.GetResourceIdentifier(logicalDevice);

			Assert(!pendingMeshAccelerationStructureData.m_scratchBuffer.IsValid());
			pendingMeshAccelerationStructureData.m_scratchBuffer = Buffer(
				logicalDevice,
				logicalDevice.GetPhysicalDevice(),
				logicalDevice.GetDeviceMemoryPool(),
				accelerationStructureBuildSizes.buildScratchSize, // TODO: updateScratchSize if updating
				Buffer::UsageFlags::StorageBuffer | Buffer::UsageFlags::ShaderDeviceAddress,
				MemoryFlags::DeviceLocal | MemoryFlags::AllocateDeviceAddress
			);

			{
				const AccelerationStructureCommandEncoder accelerationStructureCommandEncoder = graphicsCommandEncoder.BeginAccelerationStructure();

				accelerationStructureCommandEncoder.Build(
					logicalDevice,
					accelerationStructure,
					primitiveDescriptor,
					pendingMeshAccelerationStructureData.m_scratchBuffer,
					0,
					queuedGeometryBuild.m_queuedMeshBuildRanges
				);
			}

			if (meshAccelerationStructure.IsValid())
			{
				thread.GetRenderData().DestroyAccelerationStructure(logicalDevice.GetIdentifier(), Move(meshAccelerationStructure));
			}
			meshAccelerationStructure = Move(accelerationStructure);

			// TODO: Release the scratch buffer after execution finishes

			{
				InstanceBuffer::InstanceIndexType minimumInstanceIndex = Math::NumericLimits<InstanceBuffer::InstanceIndexType>::Max;
				InstanceBuffer::InstanceIndexType maximumInstanceIndex = Math::NumericLimits<InstanceBuffer::InstanceIndexType>::Min;
				for (const uint32 renderItemIndex :
				     pendingMeshAccelerationStructureData.m_pendingRenderItems.GetSetBitsIterator(0, maximumUsedRenderItemCount))
				{
					const Entity::RenderItemIdentifier renderItemIdentifier = Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex);
					const InstanceBuffer::InstanceIndexType renderItemInstanceIndex = m_instanceIndices[renderItemIdentifier];
					minimumInstanceIndex = Math::Min(renderItemInstanceIndex, minimumInstanceIndex);
					maximumInstanceIndex = Math::Max(renderItemInstanceIndex, maximumInstanceIndex);
				}

				const InstanceBuffer::InstanceIndexType totalAffectedItemCount = maximumInstanceIndex - minimumInstanceIndex + 1;

				pendingMeshAccelerationStructureData.m_stagingBuffer = StagingBuffer(
					logicalDevice,
					logicalDevice.GetPhysicalDevice(),
					logicalDevice.GetDeviceMemoryPool(),
					totalAffectedItemCount * sizeof(InstanceAccelerationStructure::Instance),
					StagingBuffer::Flags::TransferSource
				);
				const AccelerationStructureView::ResourceIdentifier accelerationStructureResourceIdentifier =
					m_meshAccelerationStructureData[meshIdentifier].m_resourceIdentifier;

				pendingMeshAccelerationStructureData.m_stagingBuffer.MapToHostMemory(
					logicalDevice,
					Math::Range<size>::Make(0, totalAffectedItemCount * sizeof(InstanceAccelerationStructure::Instance)),
					Buffer::MapMemoryFlags::Write,
					[this,
				   &staticMeshIdentifierSceneData,
				   &materialInstanceIdentifierSceneData,
				   totalAffectedItemCount,
				   accelerationStructureResourceIdentifier,
				   &pendingMeshAccelerationStructureData,
				   maximumUsedRenderItemCount,
				   minimumInstanceIndex,
				   &worldTransformSceneData,
				   &materialCache,
				   &materialInstanceCache](
						[[maybe_unused]] const Buffer::MapMemoryStatus status,
						const ByteView targetStagingBufferData,
						[[maybe_unused]] const bool wasLoadedAsynchronously
					)
					{
						Assert(status == Buffer::MapMemoryStatus::Success);
						if (LIKELY(status == Buffer::MapMemoryStatus::Success))
						{
							const ArrayView<InstanceAccelerationStructure::Instance, size> instances{
								reinterpret_cast<InstanceAccelerationStructure::Instance*>(targetStagingBufferData.GetData()),
								totalAffectedItemCount
							};

							for (const uint32 renderItemIndex :
						       pendingMeshAccelerationStructureData.m_pendingRenderItems.GetSetBitsIterator(0, maximumUsedRenderItemCount))
							{
								const Entity::RenderItemIdentifier renderItemIdentifier = Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex);
								const Entity::ComponentIdentifier componentIdentifier =
									m_sceneView.GetVisibleRenderItemComponentIdentifier(renderItemIdentifier);
								Assert(renderItemIdentifier.IsValid());
								if (UNLIKELY_ERROR(renderItemIdentifier.IsInvalid()))
								{
									continue;
								}

								const Optional<Entity::Data::RenderItem::StaticMeshIdentifier*> pStaticMeshIdentifierComponent =
									staticMeshIdentifierSceneData.GetComponentImplementation(componentIdentifier);
								Assert(pStaticMeshIdentifierComponent.IsValid());
								if (pStaticMeshIdentifierComponent.IsInvalid())
								{
									continue;
								}

								const Optional<Entity::Data::RenderItem::MaterialInstanceIdentifier*> pMaterialInstanceIdentifier =
									materialInstanceIdentifierSceneData.GetComponentImplementation(componentIdentifier);
								Assert(pMaterialInstanceIdentifier.IsValid());
								if (pMaterialInstanceIdentifier.IsInvalid())
								{
									continue;
								}
								const Rendering::MaterialInstanceIdentifier materialInstanceIdentifier = *pMaterialInstanceIdentifier;

								const Math::WorldTransform worldTransform = worldTransformSceneData.GetComponentImplementationUnchecked(componentIdentifier
							  );

								const uint8 visibilityMask = Math::NumericLimits<uint8>::Max;

								const Math::Matrix3x4f matrix(worldTransform);

								EnumFlags<InstanceAccelerationStructure::Instance::Flags> instanceFlags{};

								const Optional<const Rendering::RuntimeMaterialInstance*> pMaterialInstance =
									materialInstanceCache.GetMaterialInstance(materialInstanceIdentifier);
								if (pMaterialInstance.IsValid() && pMaterialInstance->IsValid())
								{
									const Optional<const Rendering::RuntimeMaterial*> pMaterial =
										materialCache.GetMaterial(pMaterialInstance->GetMaterialIdentifier()).Get();
									if (pMaterial.IsValid())
									{
										if (const Optional<const Rendering::MaterialAsset*> pMaterialAsset = *pMaterial->GetAsset())
										{
											instanceFlags |=
												(InstanceAccelerationStructure::Instance::Flags::DisableTriangleFaceCulling * pMaterialAsset->m_twoSided);
										}
									}
								}

								const InstanceBuffer::InstanceIndexType renderItemInstanceIndex = m_instanceIndices[renderItemIdentifier];

								const uint32 index = renderItemInstanceIndex - minimumInstanceIndex;
								instances[index] = InstanceAccelerationStructure::Instance{
									matrix,
									renderItemIdentifier.GetFirstValidIndex(),
									visibilityMask,
									instanceFlags,
									accelerationStructureResourceIdentifier
								};
							}
						}
					}
				);

				m_instanceBuffer.StartBatchUpload(
					minimumInstanceIndex,
					totalAffectedItemCount,
					graphicsCommandEncoder,
					sizeof(InstanceAccelerationStructure::Instance)
				);

				const Rendering::BufferView instanceBuffer = m_instanceBuffer.GetBuffer();

				Bitset<MaximumInstanceCount> changedInstances;
				for (const uint32 renderItemIndex :
				     pendingMeshAccelerationStructureData.m_pendingRenderItems.GetSetBitsIterator(0, maximumUsedRenderItemCount))
				{
					const Entity::RenderItemIdentifier renderItemIdentifier = Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex);
					const InstanceBuffer::InstanceIndexType renderItemInstanceIndex = m_instanceIndices[renderItemIdentifier];
					changedInstances.Set((uint16)renderItemInstanceIndex);
				}

				{
					BlitCommandEncoder blitCommandEncoder = graphicsCommandEncoder.BeginBlit();
					const BufferView targetBuffer = pendingMeshAccelerationStructureData.m_stagingBuffer;
					changedInstances.IterateSetBitRanges(
						[targetStagingBuffer = (BufferView)targetBuffer,
					   blitCommandEncoder = (BlitCommandEncoderView)blitCommandEncoder,
					   instanceBuffer,
					   minimumInstanceIndex](const Math::Range<uint16> setBits) mutable
						{
							const uint32 instanceIndex = setBits.GetMinimum();
							const uint32 instanceCount = setBits.GetSize();
							const size startTargetStagingBufferOffset = sizeof(InstanceAccelerationStructure::Instance) *
						                                              (instanceIndex - minimumInstanceIndex);

							blitCommandEncoder.RecordCopyBufferToBuffer(
								targetStagingBuffer,
								instanceBuffer,
								ArrayView<const BufferCopy, uint16>(BufferCopy{
									startTargetStagingBufferOffset,
									instanceIndex * sizeof(InstanceAccelerationStructure::Instance),
									sizeof(InstanceAccelerationStructure::Instance) * instanceCount
								})
							);
						}
					);
				}

				m_instanceBuffer.EndBatchUpload(
					minimumInstanceIndex,
					totalAffectedItemCount,
					graphicsCommandEncoder,
					sizeof(InstanceAccelerationStructure::Instance)
				);
			}

			m_buildingMeshes.Clear(meshIdentifier);
			m_builtMeshes.Set(meshIdentifier);
		}

		m_queuedGeometryBuilds.Clear();
		m_shouldRebuildInstances = true;
#else
		UNUSED(graphicsCommandEncoder);
#endif
	}

	void BuildAccelerationStructureStage::BuildInstances(const Rendering::CommandEncoderView graphicsCommandEncoder)
	{
#if RENDERER_SUPPORTS_RAYTRACING
		Rendering::LogicalDevice& logicalDevice = m_sceneView.GetLogicalDevice();

		InstanceAccelerationStructureData& __restrict instanceAccelerationStructureData = m_instancesAccelerationStructureData;
		PendingAccelerationStructureData& __restrict pendingInstanceAccelerationStructureData = instanceAccelerationStructureData.m_pendingData;

		InstanceAccelerationStructureDescriptor
			instanceDescriptor{logicalDevice, m_instanceBuffer.GetBuffer(), m_instanceBuffer.GetInstanceCount(), m_instanceCountBuffer};

		const uint32 instanceCount = m_instanceBuffer.GetInstanceCount();
		const InstanceAccelerationStructure::BuildSizesInfo accelerationStructureBuildSizes =
			instanceDescriptor.CalculateBuildSizes(logicalDevice, instanceCount);

		Threading::EngineJobRunnerThread& thread = *Threading::EngineJobRunnerThread::GetCurrent();

		if (!instanceAccelerationStructureData.m_buffer.IsValid())
		{
			instanceAccelerationStructureData.m_buffer = Buffer(
				logicalDevice,
				logicalDevice.GetPhysicalDevice(),
				logicalDevice.GetDeviceMemoryPool(),
				accelerationStructureBuildSizes.accelerationStructureSize,
				Buffer::UsageFlags::AccelerationStructureStorage | Buffer::UsageFlags::ShaderDeviceAddress,
				MemoryFlags::DeviceLocal | MemoryFlags::AllocateDeviceAddress
			);
		}
		else if (instanceAccelerationStructureData.m_buffer.GetSize() < accelerationStructureBuildSizes.accelerationStructureSize)
		{
			thread.GetRenderData().DestroyBuffer(logicalDevice.GetIdentifier(), Move(instanceAccelerationStructureData.m_buffer));
			instanceAccelerationStructureData.m_buffer = Buffer(
				logicalDevice,
				logicalDevice.GetPhysicalDevice(),
				logicalDevice.GetDeviceMemoryPool(),
				accelerationStructureBuildSizes.accelerationStructureSize,
				Buffer::UsageFlags::AccelerationStructureStorage | Buffer::UsageFlags::ShaderDeviceAddress,
				MemoryFlags::DeviceLocal | MemoryFlags::AllocateDeviceAddress
			);
		}

		InstanceAccelerationStructure accelerationStructure{
			logicalDevice,
			instanceAccelerationStructureData.m_buffer,
			instanceAccelerationStructureData.m_buffer.GetBoundMemory(),
			instanceAccelerationStructureData.m_buffer.GetOffset(),
			accelerationStructureBuildSizes.accelerationStructureSize
		};
		Assert(accelerationStructure.IsValid());

		if (!pendingInstanceAccelerationStructureData.m_scratchBuffer.IsValid())
		{
			pendingInstanceAccelerationStructureData.m_scratchBuffer = Buffer(
				logicalDevice,
				logicalDevice.GetPhysicalDevice(),
				logicalDevice.GetDeviceMemoryPool(),
				accelerationStructureBuildSizes.buildScratchSize, // TODO: updateScratchSize if updating
				Buffer::UsageFlags::StorageBuffer | Buffer::UsageFlags::ShaderDeviceAddress,
				MemoryFlags::DeviceLocal | MemoryFlags::AllocateDeviceAddress
			);
		}
		else if (pendingInstanceAccelerationStructureData.m_scratchBuffer.GetSize() < accelerationStructureBuildSizes.buildScratchSize)
		{
			thread.GetRenderData().DestroyBuffer(logicalDevice.GetIdentifier(), Move(pendingInstanceAccelerationStructureData.m_scratchBuffer));
			pendingInstanceAccelerationStructureData.m_scratchBuffer = Buffer(
				logicalDevice,
				logicalDevice.GetPhysicalDevice(),
				logicalDevice.GetDeviceMemoryPool(),
				accelerationStructureBuildSizes.buildScratchSize, // TODO: updateScratchSize if updating
				Buffer::UsageFlags::StorageBuffer | Buffer::UsageFlags::ShaderDeviceAddress,
				MemoryFlags::DeviceLocal | MemoryFlags::AllocateDeviceAddress
			);
		}

		{
			const AccelerationStructureCommandEncoder accelerationStructureCommandEncoder = graphicsCommandEncoder.BeginAccelerationStructure();

			accelerationStructureCommandEncoder.Build(
				logicalDevice,
				accelerationStructure,
				instanceDescriptor,
				pendingInstanceAccelerationStructureData.m_scratchBuffer,
				0,
				instanceCount
			);

			if (instanceAccelerationStructureData.m_accelerationStructure)
			{
				thread.GetRenderData()
					.DestroyAccelerationStructure(logicalDevice.GetIdentifier(), Move(instanceAccelerationStructureData.m_accelerationStructure));
			}
			m_instancesAccelerationStructureData.m_accelerationStructure = Move(accelerationStructure);
		}

		// TODO: Release the scratch buffer after execution finishes
#else
		UNUSED(graphicsCommandEncoder);
#endif
	}

#if RENDERER_SUPPORTS_RAYTRACING
	IdentifierArrayView<const PrimitiveAccelerationStructure, StaticMeshIdentifier>
	BuildAccelerationStructureStage::GetMeshAccelerationStructures() const
	{
		Rendering::Renderer& renderer = System::Get<Rendering::Renderer>();
		Rendering::MeshCache& meshCache = renderer.GetMeshCache();
		return meshCache.GetValidElementView(m_meshAccelerationStructures.GetView());
	}
#endif
}
