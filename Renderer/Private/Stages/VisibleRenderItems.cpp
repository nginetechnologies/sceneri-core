#include "Stages/VisibleRenderItems.h"
#include "Stages/VisibleStaticMeshes.h"
#include "Stages/PerFrameStagingBuffer.h"

#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/Memory/AddressOf.h>

#include <Engine/Entity/Data/RenderItem/StaticMeshIdentifier.h>
#include <Engine/Entity/Data/RenderItem/MaterialInstanceIdentifier.h>
#include <Engine/Entity/Data/RenderItem/VisibilityListener.h>
#include <Engine/Entity/Data/Flags.h>
#include <Engine/Entity/Scene/SceneRegistry.h>
#include <Engine/Entity/ComponentTypeSceneData.h>
#include <Engine/Threading/JobManager.h>
#include <Engine/Threading/JobRunnerThread.h>
#include <Engine/Scene/SceneBase.h>

#include <Renderer/Renderer.h>
#include <Renderer/Commands/CommandEncoderView.h>
#include <Renderer/Commands/BlitCommandEncoder.h>
#include <Renderer/Assets/StaticMesh/StaticMesh.h>
#include <Renderer/Assets/StaticMesh/MeshCache.h>
#include <Renderer/Assets/Material/RuntimeMaterialInstance.h>
#include <Renderer/Scene/SceneViewBase.h>
#include <Renderer/Buffers/StagingBuffer.h>

namespace ngine::Rendering
{
	VisibleRenderItems::~VisibleRenderItems() = default;

	void VisibleRenderItems::Destroy(LogicalDevice& logicalDevice)
	{
		for (UniquePtr<InstanceGroup>& instanceGroup : m_instanceGroupIdentifiers.GetValidElementView(m_visibleInstanceGroups.GetView()))
		{
			if (instanceGroup.IsValid())
			{
				instanceGroup->Destroy(logicalDevice);
				instanceGroup.DestroyElement();
			}
		}
	}

	void VisibleRenderItems::InstanceGroup::Destroy(LogicalDevice& logicalDevice)
	{
		Threading::JobRunnerThread::GetCurrent()->QueueCallbackFromThread(
			Threading::JobPriority::DeallocateResourcesMax,
			[instanceBuffer = Move(m_instanceBuffer), &logicalDevice](Threading::JobRunnerThread&) mutable
			{
				instanceBuffer.Destroy(logicalDevice);
			}
		);
	}

	void VisibleRenderItems::AddRenderItems(
		SceneViewBase& sceneView,
		SceneBase& scene,
		LogicalDevice& logicalDevice,
		const SceneRenderStageIdentifier stageIdentifier,
		const Entity::RenderItemMask& renderItems,
		const uint32 maximumInstanceCount,
		const Rendering::CommandEncoderView graphicsCommandEncoder,
		PerFrameStagingBuffer& perFrameStagingBuffer

	)
	{
		Entity::SceneRegistry& sceneRegistry = scene.GetEntitySceneRegistry();

		Entity::RenderItemMask addedRenderItems;
		const typename Entity::RenderItemIdentifier::IndexType maximumUsedRenderItemCount = scene.GetMaximumUsedRenderItemCount();

		// TODO: Could probably optimize this by filtering by instance group and doing one group at a time
		// Then we can do one allocation for all buffers (per instance group)

		for (const uint32 renderItemIndex : renderItems.GetSetBitsIterator(0, maximumUsedRenderItemCount))
		{
			const Entity::RenderItemIdentifier renderItemIdentifier = Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex);
			const Optional<Entity::HierarchyComponentBase*> pVisibleComponent =
				sceneView.GetVisibleRenderItemComponent(Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex));
			if (UNLIKELY(pVisibleComponent.IsInvalid()))
			{
				continue;
			}

			const VisibleInstanceGroupIdentifier visibleInstanceGroupIdentifier =
				FindOrCreateInstanceGroup(logicalDevice, *pVisibleComponent, sceneRegistry, maximumInstanceCount);
			if (!visibleInstanceGroupIdentifier.IsValid())
			{
				continue;
			}

			InstanceGroup& instanceGroup = *m_visibleInstanceGroups[visibleInstanceGroupIdentifier];
			const InstanceBuffer::InstanceIndexType instanceIndex = instanceGroup.m_instanceBuffer.AddInstance();
			Assert(instanceIndex < instanceGroup.m_instanceBuffer.GetCapacity());
			if (UNLIKELY(instanceIndex == instanceGroup.m_instanceBuffer.GetCapacity()))
			{
				continue;
			}

			m_renderItemInfo[renderItemIdentifier] = RenderItemInfo{visibleInstanceGroupIdentifier.GetIndex(), instanceIndex};
			m_instanceCount++;

			addedRenderItems.Set(renderItemIdentifier);
		}

		if (addedRenderItems.AreAnySet())
		{
			AddRenderItemsInternal(scene, logicalDevice, addedRenderItems, graphicsCommandEncoder, perFrameStagingBuffer);

			for (const uint32 renderItemIndex : addedRenderItems.GetSetBitsIterator(0, maximumUsedRenderItemCount))
			{
				sceneView.GetSubmittedRenderItemStageMask(Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex)).Set(stageIdentifier);
			}
		}
	}

	void VisibleRenderItems::ResetRenderItems(
		SceneViewBase& sceneView,
		SceneBase& scene,
		LogicalDevice& logicalDevice,
		const SceneRenderStageIdentifier stageIdentifier,
		const Entity::RenderItemMask& renderItems,
		const uint32 maximumInstanceCount,
		const Rendering::CommandEncoderView graphicsCommandEncoder,
		PerFrameStagingBuffer& perFrameStagingBuffer

	)
	{
		RemoveRenderItems(logicalDevice, renderItems, scene, graphicsCommandEncoder, perFrameStagingBuffer);

		Entity::SceneRegistry& sceneRegistry = scene.GetEntitySceneRegistry();

		// Now reapply the item
		Entity::RenderItemMask addedRenderItems;
		const typename Entity::RenderItemIdentifier::IndexType maximumUsedRenderItemCount = scene.GetMaximumUsedRenderItemCount();

		// TODO: Could probably optimize this by filtering by instance group and doing one group at a time
		// Then we can do one allocation for all buffers (per instance group)

		for (const uint32 renderItemIndex : renderItems.GetSetBitsIterator(0, maximumUsedRenderItemCount))
		{
			const Entity::RenderItemIdentifier renderItemIdentifier = Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex);
			const Optional<Entity::HierarchyComponentBase*> pVisibleComponent =
				sceneView.GetVisibleRenderItemComponent(Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex));
			Assert(pVisibleComponent.IsValid());
			if (UNLIKELY_ERROR(pVisibleComponent.IsInvalid()))
			{
				continue;
			}

			const VisibleInstanceGroupIdentifier visibleInstanceGroupIdentifier =
				FindOrCreateInstanceGroup(logicalDevice, *pVisibleComponent, sceneRegistry, maximumInstanceCount);
			if (!visibleInstanceGroupIdentifier.IsValid())
			{
				sceneView.GetQueuedRenderItemStageMask(renderItemIdentifier).Clear(stageIdentifier);
				sceneView.GetSubmittedRenderItemStageMask(renderItemIdentifier).Clear(stageIdentifier);
				continue;
			}

			InstanceGroup& instanceGroup = *m_visibleInstanceGroups[visibleInstanceGroupIdentifier];
			const InstanceBuffer::InstanceIndexType instanceIndex = instanceGroup.m_instanceBuffer.AddInstance();
			Assert(instanceIndex < instanceGroup.m_instanceBuffer.GetCapacity());
			if (UNLIKELY(instanceIndex == instanceGroup.m_instanceBuffer.GetCapacity()))
			{
				sceneView.GetQueuedRenderItemStageMask(renderItemIdentifier).Clear(stageIdentifier);
				sceneView.GetSubmittedRenderItemStageMask(renderItemIdentifier).Clear(stageIdentifier);
				continue;
			}

			m_renderItemInfo[renderItemIdentifier] = RenderItemInfo{visibleInstanceGroupIdentifier.GetIndex(), instanceIndex};

			addedRenderItems.Set(renderItemIdentifier);
		}

		if (addedRenderItems.AreAnySet())
		{
			AddRenderItemsInternal(scene, logicalDevice, addedRenderItems, graphicsCommandEncoder, perFrameStagingBuffer);

			for (const uint32 renderItemIndex : addedRenderItems.GetSetBitsIterator(0, maximumUsedRenderItemCount))
			{
				sceneView.GetSubmittedRenderItemStageMask(Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex)).Set(stageIdentifier);
			}
		}
	}

	void VisibleRenderItems::RemoveRenderItems(
		LogicalDevice& logicalDevice,
		const Entity::RenderItemMask& renderItems,
		SceneBase& scene,
		const Rendering::CommandEncoderView graphicsCommandEncoder,
		PerFrameStagingBuffer& perFrameStagingBuffer
	)
	{
		const typename Entity::RenderItemIdentifier::IndexType maximumUsedRenderItemCount = scene.GetMaximumUsedRenderItemCount();
		for (const uint32 renderItemIndex : renderItems.GetSetBitsIterator(0, maximumUsedRenderItemCount))
		{
			RenderItemInfo& renderItemInfo = m_renderItemInfo[Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex)];
			const VisibleInstanceGroupIdentifier visibleInstanceGroupIdentifier =
				VisibleInstanceGroupIdentifier::MakeFromIndex(renderItemInfo.m_visibleInstanceGroupIdentifierIndex);
			if (LIKELY(visibleInstanceGroupIdentifier.IsValid()))
			{
				InstanceGroup& instanceGroup = *m_visibleInstanceGroups[visibleInstanceGroupIdentifier];

				Assert(
					(renderItemInfo.m_instanceIndex >= instanceGroup.m_instanceBuffer.GetFirstInstanceIndex()) &
					(renderItemInfo.m_instanceIndex - instanceGroup.m_instanceBuffer.GetFirstInstanceIndex() <
				   instanceGroup.m_instanceBuffer.GetInstanceCount())
				);

				RemoveRenderItemFromInstanceGroup(
					logicalDevice,
					instanceGroup,
					visibleInstanceGroupIdentifier,
					renderItemInfo,
					scene,
					graphicsCommandEncoder,
					perFrameStagingBuffer
				);
			}
		}
	}

	void VisibleRenderItems::AddRenderItemsInternal(
		SceneBase& scene,
		LogicalDevice& logicalDevice,
		const Entity::RenderItemMask& renderItems,
		const Rendering::CommandEncoderView graphicsCommandEncoder,
		PerFrameStagingBuffer& perFrameStagingBuffer

	)
	{
		Entity::RenderItemMask renderItemsTemp = renderItems;
		Entity::RenderItemMask instanceGroupRenderItems;
		Entity::RenderItemMask invalidRenderItems;

		const typename Entity::RenderItemIdentifier::IndexType maximumUsedRenderItemCount = scene.GetMaximumUsedRenderItemCount();

		do
		{
			InstanceGroup* pInstanceGroup = nullptr;
			do
			{
				const Memory::BitIndex<uint32> firstRenderItemIndexOptional = renderItemsTemp.GetFirstSetIndex();
				if (firstRenderItemIndexOptional.IsInvalid())
				{
					return;
				}
				const uint32 firstRenderItemIndex = *firstRenderItemIndexOptional;

				RenderItemInfo& renderItemInfo = m_renderItemInfo[Entity::RenderItemIdentifier::MakeFromValidIndex(firstRenderItemIndex)];
				const VisibleInstanceGroupIdentifier visibleInstanceGroupIdentifier =
					VisibleInstanceGroupIdentifier::MakeFromIndex(renderItemInfo.m_visibleInstanceGroupIdentifierIndex);
				if (UNLIKELY(!visibleInstanceGroupIdentifier.IsValid()))
				{
					renderItemsTemp.Clear(Entity::RenderItemIdentifier::MakeFromValidIndex(firstRenderItemIndex));
					continue;
				}
				pInstanceGroup = &*m_visibleInstanceGroups[visibleInstanceGroupIdentifier];
				if (pInstanceGroup != nullptr)
				{
					instanceGroupRenderItems.Set(Entity::RenderItemIdentifier::MakeFromValidIndex(firstRenderItemIndex));
				}
			} while (pInstanceGroup == nullptr);

			for (const uint32 renderItemIndex : renderItemsTemp.GetSetBitsIterator(0, maximumUsedRenderItemCount))
			{
				RenderItemInfo& renderItemInfo = m_renderItemInfo[Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex)];
				const VisibleInstanceGroupIdentifier visibleInstanceGroupIdentifier =
					VisibleInstanceGroupIdentifier::MakeFromIndex(renderItemInfo.m_visibleInstanceGroupIdentifierIndex);
				if (UNLIKELY(!visibleInstanceGroupIdentifier.IsValid()))
				{
					invalidRenderItems.Set(Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex));
					continue;
				}
				InstanceGroup& instanceGroup = *m_visibleInstanceGroups[visibleInstanceGroupIdentifier];
				if (&instanceGroup == pInstanceGroup)
				{
					instanceGroupRenderItems.Set(Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex));
				}
			}

			uint32 minimumInstanceIndex = Math::NumericLimits<uint32>::Max;
			uint32 maximumInstanceIndex = Math::NumericLimits<uint32>::Min;
			for (const uint32 renderItemIndex : instanceGroupRenderItems.GetSetBitsIterator(0, maximumUsedRenderItemCount))
			{
				const RenderItemInfo& __restrict renderItemInfo =
					m_renderItemInfo[Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex)];
				minimumInstanceIndex = Math::Min(renderItemInfo.m_instanceIndex, minimumInstanceIndex);
				maximumInstanceIndex = Math::Max(renderItemInfo.m_instanceIndex, maximumInstanceIndex);
			}

			const uint32 totalAffectedItemCount = maximumInstanceIndex - minimumInstanceIndex + 1;
			pInstanceGroup->m_instanceBuffer
				.StartBatchUpload(minimumInstanceIndex, totalAffectedItemCount, graphicsCommandEncoder, sizeof(InstanceGroup::IdentifierIndexType));

			const Rendering::BufferView instanceIdentifierBuffer = pInstanceGroup->m_instanceBuffer.GetBuffer();

			constexpr uint32 MaximumInstanceCount = 100000;
			Bitset<MaximumInstanceCount> changedInstances;
			for (const uint32 renderItemIndex : instanceGroupRenderItems.GetSetBitsIterator(0, maximumUsedRenderItemCount))
			{
				const RenderItemInfo& __restrict renderItemInfo =
					m_renderItemInfo[Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex)];
				changedInstances.Set(renderItemInfo.m_instanceIndex);
				m_tempInstanceRenderItems[renderItemInfo.m_instanceIndex - minimumInstanceIndex] = renderItemIndex;
			}

			const CommandQueueView graphicsCommandQueue = logicalDevice.GetCommandQueue(QueueFamily::Graphics);
			changedInstances.IterateSetBitRanges(
				[&perFrameStagingBuffer,
			   instanceIdentifierBuffer,
			   minimumInstanceIndex,
			   instanceRenderItems = m_tempInstanceRenderItems.GetView(),
			   graphicsCommandQueue,
			   &logicalDevice,
			   graphicsCommandEncoder](const Math::Range<uint32> setBits) mutable
				{
					const uint32 firstInstanceIndex = setBits.GetMinimum();
					const uint32 instanceCount = setBits.GetSize();
					PerFrameStagingBuffer::BatchCopyContext copyInstanceIdentifiersContext = perFrameStagingBuffer.BeginBatchCopyToBuffer(
						sizeof(InstanceGroup::IdentifierIndexType) * instanceCount,
						instanceIdentifierBuffer,
						firstInstanceIndex * sizeof(InstanceGroup::IdentifierIndexType)
					);

					uint32 localIndex = 0;
					for (const uint32 instanceIndex : setBits)
					{
						const uint32 index = instanceIndex - minimumInstanceIndex;
						const Entity::RenderItemIdentifier::IndexType renderItemIndex = instanceRenderItems[index];

						perFrameStagingBuffer.BatchCopyToBuffer(
							logicalDevice,
							copyInstanceIdentifiersContext,
							graphicsCommandQueue,
							ConstByteView::Make(renderItemIndex),
							localIndex * sizeof(InstanceGroup::IdentifierIndexType)
						);
						localIndex++;
					}

					perFrameStagingBuffer.EndBatchCopyToBuffer(copyInstanceIdentifiersContext, graphicsCommandEncoder);
				}
			);

			pInstanceGroup->m_instanceBuffer
				.EndBatchUpload(minimumInstanceIndex, totalAffectedItemCount, graphicsCommandEncoder, sizeof(InstanceGroup::IdentifierIndexType));

			renderItemsTemp.Clear(instanceGroupRenderItems);
			renderItemsTemp.Clear(invalidRenderItems);
			instanceGroupRenderItems.ClearAll();
		} while (renderItemsTemp.AreAnySet());
	}

	void VisibleRenderItems::OnSceneUnloaded(LogicalDevice& logicalDevice, SceneBase& scene)
	{
		for (UniquePtr<InstanceGroup>& instanceGroup : m_instanceGroupIdentifiers.GetValidElementView(m_visibleInstanceGroups.GetView()))
		{
			if (instanceGroup.IsValid())
			{
				instanceGroup->Destroy(logicalDevice);
				instanceGroup.DestroyElement();
			}
		}
		scene.GetValidRenderItemView(m_renderItemInfo.GetView()).ZeroInitialize();

		m_instanceGroupIdentifiers.Reset();
	}

	void VisibleRenderItems::RemoveRenderItemFromInstanceGroup(
		LogicalDevice& logicalDevice,
		InstanceGroup& instanceGroup,
		const VisibleInstanceGroupIdentifier instanceGroupIdentifier,
		RenderItemInfo& renderItemInfo,
		SceneBase& scene,
		const CommandEncoderView graphicsCommandEncoder,
		PerFrameStagingBuffer& perFrameStagingBuffer
	)
	{
		m_instanceCount--;

		switch (instanceGroup.m_instanceBuffer.RemoveInstance(renderItemInfo.m_instanceIndex))
		{
			case InstanceBuffer::RemovalResult::RemovedLastRemainingInstance:
			{
				m_instanceGroupIdentifiers.ReturnIdentifier(instanceGroupIdentifier);
				m_instanceGroupCount--;
				renderItemInfo.m_visibleInstanceGroupIdentifierIndex = VisibleInstanceGroupIdentifier::Invalid;
				instanceGroup.Destroy(logicalDevice);
				m_visibleInstanceGroups[instanceGroupIdentifier].DestroyElement();
			}
			break;
			case InstanceBuffer::RemovalResult::RemovedFrontInstance:
			{
				renderItemInfo.m_visibleInstanceGroupIdentifierIndex = VisibleInstanceGroupIdentifier::Invalid;
			}
			break;
			case InstanceBuffer::RemovalResult::RemovedBackInstance:
			{
				renderItemInfo.m_visibleInstanceGroupIdentifierIndex = VisibleInstanceGroupIdentifier::Invalid;
			}
			break;
			case InstanceBuffer::RemovalResult::RemovedMiddleInstance:
			{
				renderItemInfo.m_visibleInstanceGroupIdentifierIndex = VisibleInstanceGroupIdentifier::Invalid;

				const InstanceBuffer::InstanceIndexType lastInstanceIndex = instanceGroup.m_instanceBuffer.GetFirstInstanceIndex() +
				                                                            instanceGroup.m_instanceBuffer.GetInstanceCount();
				// Move the last instance to the now unused index
				const RenderItems::DynamicView renderItems = scene.GetValidRenderItemView(m_renderItemInfo.GetView());
				for (RenderItemInfo& otherRenderItemInfo : renderItems)
				{
					if (otherRenderItemInfo.m_visibleInstanceGroupIdentifierIndex != instanceGroupIdentifier.GetIndex())
					{
						continue;
					}

					const InstanceBuffer::InstanceIndexType previousInstanceIndex = otherRenderItemInfo.m_instanceIndex;
					if (previousInstanceIndex == lastInstanceIndex)
					{
						const InstanceBuffer::InstanceIndexType newInstanceIndex = renderItemInfo.m_instanceIndex;
						otherRenderItemInfo.m_instanceIndex = newInstanceIndex;

						instanceGroup.m_instanceBuffer.MoveElement(
							lastInstanceIndex,
							otherRenderItemInfo.m_instanceIndex,
							graphicsCommandEncoder,
							sizeof(InstanceGroup::IdentifierIndexType),
							perFrameStagingBuffer
						);

						return;
					}
				}

				Assert(false, "Removing instance that could not be found in instance group!");
			}
		}
	}

	VisibleRenderItems::VisibleInstanceGroupIdentifier VisibleRenderItems::FindOrCreateInstanceGroup(
		LogicalDevice& logicalDevice,
		const Entity::HierarchyComponentBase& renderItem,
		Entity::SceneRegistry& sceneRegistry,
		const uint32 maximumInstanceCount
	)
	{
		const InstanceGroupQueryResult queryExistingResult = FindInstanceGroup(logicalDevice, renderItem, sceneRegistry);
		switch (queryExistingResult.m_result)
		{
			case InstanceGroup::SupportResult::Unknown:
				return {};
			case InstanceGroup::SupportResult::Supported:
				Assert(queryExistingResult.m_identifier.IsValid());
				return queryExistingResult.m_identifier;
			case InstanceGroup::SupportResult::Unsupported:
				return CreateInstanceGroupInternal(logicalDevice, renderItem, sceneRegistry, maximumInstanceCount);
		}
		ExpectUnreachable();
	}

	VisibleRenderItems::VisibleInstanceGroupIdentifier VisibleRenderItems::CreateInstanceGroupInternal(
		LogicalDevice& logicalDevice,
		const Entity::HierarchyComponentBase& renderItem,
		Entity::SceneRegistry& sceneRegistry,
		const uint32 maximumInstanceCount
	)
	{
		UniquePtr<InstanceGroup> pInstanceGroup = CreateInstanceGroup(logicalDevice, renderItem, sceneRegistry, maximumInstanceCount);
		if (LIKELY(pInstanceGroup.IsValid()))
		{
			Assert(pInstanceGroup->m_instanceBuffer.IsValid());

			const VisibleInstanceGroupIdentifier instanceGroupIdentifier = m_instanceGroupIdentifiers.AcquireIdentifier();
			Assert(instanceGroupIdentifier.IsValid());
			if (LIKELY(instanceGroupIdentifier.IsValid()))
			{
				Assert(m_visibleInstanceGroups[instanceGroupIdentifier].IsInvalid());
				m_visibleInstanceGroups[instanceGroupIdentifier] = Move(pInstanceGroup);
				m_instanceGroupCount++;
				return instanceGroupIdentifier;
			}
		}
		return {};
	}

	VisibleRenderItems::InstanceGroupQueryResult VisibleRenderItems::FindInstanceGroup(
		const LogicalDevice& logicalDevice, const Entity::HierarchyComponentBase& renderItem, Entity::SceneRegistry& sceneRegistry
	) const
	{
		for (const UniquePtr<InstanceGroup>& instanceGroup : m_instanceGroupIdentifiers.GetValidElementView(m_visibleInstanceGroups.GetView()))
		{
			if (instanceGroup != nullptr)
			{
				switch (instanceGroup->SupportsComponent(logicalDevice, renderItem, sceneRegistry))
				{
					case InstanceGroup::SupportResult::Unknown:
						return {{}, InstanceGroup::SupportResult::Unknown};
					case InstanceGroup::SupportResult::Supported:
					{
						const VisibleInstanceGroupIdentifier instanceIdentifier = VisibleInstanceGroupIdentifier::MakeFromValidIndex(
							m_visibleInstanceGroups.GetView().GetIteratorIndex(Memory::GetAddressOf(instanceGroup))
						);
						return {m_instanceGroupIdentifiers.GetActiveIdentifier(instanceIdentifier), InstanceGroup::SupportResult::Supported};
					}
					case InstanceGroup::SupportResult::Unsupported:
						break;
				}
			}
		}

		return {{}, InstanceGroup::SupportResult::Unsupported};
	}

	VisibleRenderItems::InstanceGroup::SupportResult VisibleStaticMeshes::InstanceGroup::SupportsComponent(
		const LogicalDevice&, const Entity::HierarchyComponentBase& component, Entity::SceneRegistry& sceneRegistry
	) const
	{
		const Entity::ComponentIdentifier componentIdentifier = component.GetIdentifier();
		Entity::ComponentTypeSceneData<Entity::Data::RenderItem::StaticMeshIdentifier>& staticMeshIdentifierSceneData =
			sceneRegistry.GetCachedSceneData<Entity::Data::RenderItem::StaticMeshIdentifier>();

		const Rendering::StaticMeshIdentifier meshIdentifier =
			staticMeshIdentifierSceneData.GetComponentImplementationUnchecked(componentIdentifier);
		return (meshIdentifier.IsValid() && m_meshIdentifierIndex == meshIdentifier.GetFirstValidIndex()) ? SupportResult::Supported
		                                                                                                  : SupportResult::Unsupported;
	}

	UniquePtr<VisibleRenderItems::InstanceGroup> VisibleStaticMeshes::CreateInstanceGroup(
		LogicalDevice& logicalDevice,
		const Entity::HierarchyComponentBase& renderItem,
		Entity::SceneRegistry& sceneRegistry,
		const uint32 maximumInstanceCount
	)
	{
		Entity::ComponentTypeSceneData<Entity::Data::RenderItem::StaticMeshIdentifier>& staticMeshIdentifierSceneData =
			sceneRegistry.GetCachedSceneData<Entity::Data::RenderItem::StaticMeshIdentifier>();
		Entity::ComponentTypeSceneData<Entity::Data::RenderItem::MaterialInstanceIdentifier>& materialInstanceIdentifierSceneData =
			sceneRegistry.GetCachedSceneData<Entity::Data::RenderItem::MaterialInstanceIdentifier>();
		const Entity::ComponentIdentifier componentIdentifier = renderItem.GetIdentifier();
		const Rendering::MaterialInstanceIdentifier materialInstanceIdentifier =
			materialInstanceIdentifierSceneData.GetComponentImplementationUnchecked(componentIdentifier);

		Rendering::Renderer& renderer = logicalDevice.GetRenderer();
		Rendering::MeshCache& meshCache = renderer.GetMeshCache();
		Rendering::MaterialCache& materialCache = renderer.GetMaterialCache();
		Rendering::RuntimeMaterialInstance& materialInstance = *materialCache.GetInstanceCache().GetMaterialInstance(materialInstanceIdentifier
		);

		const Rendering::StaticMeshIdentifier meshIdentifier =
			staticMeshIdentifierSceneData.GetComponentImplementationUnchecked(componentIdentifier);
		const Optional<const StaticMesh*> pMesh = *meshCache.GetAssetData(meshIdentifier).m_pMesh;
		if (materialInstance.IsValid() && pMesh.IsValid())
		{
			UniquePtr<InstanceGroup> pInstanceGroup = UniquePtr<InstanceGroup>::Make(logicalDevice, maximumInstanceCount);
			if (LIKELY(pInstanceGroup->m_instanceBuffer.IsValid()))
			{
				pInstanceGroup->m_meshIdentifierIndex = meshIdentifier.GetFirstValidIndex();
				Threading::JobBatch jobBatch = renderer.GetMeshCache().TryLoadRenderMesh(
					logicalDevice.GetIdentifier(),
					meshIdentifier,
					Rendering::MeshCache::RenderMeshLoadListenerData{
						*pInstanceGroup,
						[](InstanceGroup& instanceGroup, RenderMeshView newRenderMesh, const EnumFlags<MeshCache::LoadedMeshFlags>)
						{
							instanceGroup.m_renderMeshView = newRenderMesh;
							return EventCallbackResult::Keep;
						}
					}
				);

				Entity::ComponentTypeSceneData<Entity::Data::RenderItem::VisibilityListener>& visibilityListenerSceneData =
					sceneRegistry.GetCachedSceneData<Entity::Data::RenderItem::VisibilityListener>();
				if (const Optional<Entity::Data::RenderItem::VisibilityListener*> pVisibilityListener = visibilityListenerSceneData.GetComponentImplementation(componentIdentifier))
				{
					pVisibilityListener->Invoke(logicalDevice, jobBatch);
				}

				if (jobBatch.IsValid())
				{
					Threading::JobRunnerThread::GetCurrent()->Queue(jobBatch);
				}

				return pInstanceGroup;
			}

			if (pInstanceGroup.IsValid())
			{
				pInstanceGroup->Destroy(logicalDevice);
			}
		}

		return nullptr;
	}

	Threading::JobBatch VisibleStaticMeshes::TryLoadRenderItemResources(
		LogicalDevice& logicalDevice, const Entity::HierarchyComponentBase& renderItem, Entity::SceneRegistry& sceneRegistry
	)
	{
		Entity::ComponentTypeSceneData<Entity::Data::RenderItem::StaticMeshIdentifier>& staticMeshIdentifierSceneData =
			sceneRegistry.GetCachedSceneData<Entity::Data::RenderItem::StaticMeshIdentifier>();

		const Entity::ComponentIdentifier componentIdentifier = renderItem.GetIdentifier();

		const Rendering::StaticMeshIdentifier staticMeshIdentifier =
			staticMeshIdentifierSceneData.GetComponentImplementationUnchecked(componentIdentifier);
		Threading::IntermediateStage& finishedStage = Threading::CreateIntermediateStage();
		Threading::JobBatch jobBatch = Threading::JobBatch::IntermediateStage;
		finishedStage.AddSubsequentStage(jobBatch.GetFinishedStage());

		return logicalDevice.GetRenderer().GetMeshCache().TryLoadRenderMesh(
			logicalDevice.GetIdentifier(),
			staticMeshIdentifier,
			MeshCache::RenderMeshLoadListenerData(
				&finishedStage,
				[&finishedStage](Threading::IntermediateStage&, const RenderMeshView&, const EnumFlags<MeshCache::LoadedMeshFlags>)
				{
					finishedStage.SignalExecutionFinishedAndDestroying(*Threading::JobRunnerThread::GetCurrent());
					return EventCallbackResult::Remove;
				}
			),
			MeshLoadFlags::Default & ~MeshLoadFlags::LoadDummy
		);
	}

	void VisibleStaticMeshes::InstanceGroup::Destroy(LogicalDevice& logicalDevice)
	{
		VisibleRenderItems::InstanceGroup::Destroy(logicalDevice);
		[[maybe_unused]] const bool wasRemoved = logicalDevice.GetRenderer().GetMeshCache().RemoveRenderMeshListener(
			logicalDevice.GetIdentifier(),
			StaticMeshIdentifier::MakeFromValidIndex(m_meshIdentifierIndex),
			this
		);
		m_renderMeshView = {};
	}
}
