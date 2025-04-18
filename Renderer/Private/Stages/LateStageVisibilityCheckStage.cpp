#include "Stages/LateStageVisibilityCheckStage.h"
#include "Stages/OctreeTraversalStage.h"

#include "Scene/SceneView.h"
#include "Devices/LogicalDevice.h"
#include "Jobs/QueueSubmissionJob.h"
#include "Renderer/Renderer.h"

#include <Engine/Scene/Scene.h>
#include <Engine/Entity/HierarchyComponentBase.h>

#include <Renderer/Stages/RenderItemStage.h>
#include <Renderer/Commands/CommandEncoderView.h>

#include <Common/Threading/Jobs/JobRunnerThread.h>

namespace ngine::Rendering
{
	Threading::JobBatch RenderItemStage::LoadRenderItemsResources(const Entity::RenderItemMask&)
	{
		return {};
	}

	LateStageVisibilityCheckStage::LateStageVisibilityCheckStage(SceneView& sceneView)
		: Stage(sceneView.GetLogicalDevice(), Threading::JobPriority::OctreeCulling)
		, m_sceneView(sceneView)
		, m_perFrameStagingBuffer(
				m_sceneView.GetLogicalDevice(),
				m_sceneView.GetLogicalDevice().GetPhysicalDevice(),
				m_sceneView.GetLogicalDevice().GetDeviceMemoryPool(),
				OctreeTraversalStage::StagingBufferSize,
				StagingBuffer::Flags::TransferSource | StagingBuffer::Flags::TransferDestination
			)
	{
	}

	LateStageVisibilityCheckStage::~LateStageVisibilityCheckStage()
	{
		m_perFrameStagingBuffer.Destroy(m_sceneView.GetLogicalDevice(), m_sceneView.GetLogicalDevice().GetDeviceMemoryPool());
	}

	bool LateStageVisibilityCheckStage::ShouldRecordCommands() const
	{
		return m_queuedLateStageRenderItemChanges.HasElements();
	}

	void LateStageVisibilityCheckStage::RecordCommands(const CommandEncoderView graphicsCommandEncoder)
	{
		m_perFrameStagingBuffer.Start();

		m_sceneView.StartLateStageOctreeVisibilityCheck();

		{
			Threading::UniqueLock lock(m_mutex);
			Vector<QueuedLateStageRenderItemChange> copy = Move(m_queuedLateStageRenderItemChangesCopy);
			m_queuedLateStageRenderItemChangesCopy = Move(m_queuedLateStageRenderItemChanges);
			m_queuedLateStageRenderItemChanges = Move(copy);
		}

		for (QueuedLateStageRenderItemChange& queuedChange : m_queuedLateStageRenderItemChangesCopy)
		{
			// TODO: Separate these into separate vectors
			// Better for cache most likely.
			switch (queuedChange.m_type)
			{
				case Type::LateStageRegistration:
				{
					m_sceneView.ProcessLateStageAddedRenderItem(queuedChange.m_renderItem);
				}
				break;
				case Type::LateStageTransformChange:
				{
					m_sceneView.ProcessLateStageChangedRenderItemTransform(queuedChange.m_renderItem);
				}
				break;
				case Type::RenderItemEnabled:
				{
					m_sceneView.ProcessEnabledRenderItem(queuedChange.m_renderItem);
				}
				break;
				case Type::StageEnable:
				{
					m_sceneView
						.ProcessEnabledRenderItemStage(queuedChange.m_stageInfo.m_renderItemIdentifier, queuedChange.m_stageInfo.m_stageIdentifier);
				}
				break;
				case Type::StageDisable:
				{
					m_sceneView
						.ProcessDisabledRenderItemStage(queuedChange.m_stageInfo.m_renderItemIdentifier, queuedChange.m_stageInfo.m_stageIdentifier);
				}
				break;
				case Type::StageReset:
				{
					m_sceneView
						.ProcessResetRenderItemStage(queuedChange.m_stageInfo.m_renderItemIdentifier, queuedChange.m_stageInfo.m_stageIdentifier);
				}
				break;
				case Type::StageMaskReset:
				{
					m_sceneView
						.ProcessResetRenderItemStageMask(queuedChange.m_stageMaskInfo.m_renderItemIdentifier, queuedChange.m_stageMaskInfo.m_stages);
				}
				break;
				case Type::StageMaskEnable:
				{
					m_sceneView
						.ProcessEnabledRenderItemStageMask(queuedChange.m_stageMaskInfo.m_renderItemIdentifier, queuedChange.m_stageMaskInfo.m_stages);
				}
				break;
				case Type::StageMaskDisable:
				{
					m_sceneView
						.ProcessDisabledRenderItemStageMask(queuedChange.m_stageMaskInfo.m_renderItemIdentifier, queuedChange.m_stageMaskInfo.m_stages);
				}
				break;
				case Type::StageMaskChanged:
				{
					m_sceneView.ProcessChangedRenderItemStageMask(
						queuedChange.m_changedStagemaskInfo.m_renderItemIdentifier,
						queuedChange.m_changedStagemaskInfo.m_enabledStages,
						queuedChange.m_changedStagemaskInfo.m_disabledStages
					);
				}
				break;
				case Type::ActiveCameraTransformChange:
				{
					if (LIKELY(m_sceneView.HasActiveCamera()))
					{
						m_sceneView.ProcessCameraTransformChanged(graphicsCommandEncoder, m_perFrameStagingBuffer);
					}
				}
				break;
				case Type::ActiveCameraPropertyChange:
				{
					m_sceneView.ProcessCameraPropertiesChanged(graphicsCommandEncoder, m_perFrameStagingBuffer);
				}
				break;
			}
		}
		m_queuedLateStageRenderItemChangesCopy.Clear();

		if (m_disabledRenderItems.AreAnySet())
		{
			const Entity::RenderItemMask disabledRenderItems = m_disabledRenderItems.FetchClear();
			m_sceneView.ProcessDisabledRenderItems(*m_sceneView.GetSceneChecked(), disabledRenderItems);
		}

		if (m_removedRenderItems.AreAnySet())
		{
			const Entity::RenderItemMask removedRenderItems = m_removedRenderItems.FetchClear();
			m_sceneView.ProcessRemovedRenderItems(*m_sceneView.GetSceneChecked(), removedRenderItems);
		}

		m_sceneView.NotifyOctreeTraversalRenderStages(graphicsCommandEncoder, m_perFrameStagingBuffer);
	}

	void LateStageVisibilityCheckStage::QueueChangedRenderItemTransformForLateStageCulling(Entity::HierarchyComponentBase& renderItem)
	{
		Threading::UniqueLock lock(m_mutex);
		m_queuedLateStageRenderItemChanges.EmplaceBack(QueuedLateStageRenderItemChange{Type::LateStageTransformChange, renderItem});
	}

	void LateStageVisibilityCheckStage::QueueRenderItemForLateStageAdd(Entity::HierarchyComponentBase& renderItem)
	{
		Assert(renderItem.IsEnabled());
		Threading::UniqueLock lock(m_mutex);
		m_queuedLateStageRenderItemChanges.EmplaceBack(QueuedLateStageRenderItemChange{Type::LateStageRegistration, renderItem});
	}

	void LateStageVisibilityCheckStage::QueueRenderItemForRemoval(const Entity::RenderItemIdentifier renderItemIdentifier)
	{
		m_removedRenderItems.Set(renderItemIdentifier);
	}

	void LateStageVisibilityCheckStage::QueueRenderItemEnabled(Entity::HierarchyComponentBase& renderItem)
	{
		Threading::UniqueLock lock(m_mutex);
		m_queuedLateStageRenderItemChanges.EmplaceBack(QueuedLateStageRenderItemChange{Type::RenderItemEnabled, renderItem});
	}

	void LateStageVisibilityCheckStage::QueueRenderItemDisabled(const Entity::RenderItemIdentifier renderItemIdentifier)
	{
		m_disabledRenderItems.Set(renderItemIdentifier);
	}

	void LateStageVisibilityCheckStage::QueueRenderItemForStageMaskEnable(
		const Entity::RenderItemIdentifier renderItemIdentifier, const RenderItemStageMask& stages
	)
	{
		Threading::UniqueLock lock(m_mutex);
		m_queuedLateStageRenderItemChanges.EmplaceBack(QueuedLateStageRenderItemChange{Type::StageMaskEnable, renderItemIdentifier, stages});
	}

	void LateStageVisibilityCheckStage::QueueRenderItemForStageMaskDisable(
		const Entity::RenderItemIdentifier renderItemIdentifier, const RenderItemStageMask& stages
	)
	{
		Threading::UniqueLock lock(m_mutex);
		m_queuedLateStageRenderItemChanges.EmplaceBack(QueuedLateStageRenderItemChange{Type::StageMaskDisable, renderItemIdentifier, stages});
	}

	void LateStageVisibilityCheckStage::QueueRenderItemForStageMaskChanged(
		const Entity::RenderItemIdentifier renderItemIdentifier,
		const RenderItemStageMask& enabledStages,
		const RenderItemStageMask& disabledStages
	)
	{
		Threading::UniqueLock lock(m_mutex);
		m_queuedLateStageRenderItemChanges.EmplaceBack(
			QueuedLateStageRenderItemChange{Type::StageMaskChanged, renderItemIdentifier, enabledStages, disabledStages}
		);
	}

	void LateStageVisibilityCheckStage::QueueRenderItemForStageMaskReset(
		const Entity::RenderItemIdentifier renderItemIdentifier, const RenderItemStageMask& resetStages
	)
	{
		Threading::UniqueLock lock(m_mutex);
		m_queuedLateStageRenderItemChanges.EmplaceBack(QueuedLateStageRenderItemChange{Type::StageMaskReset, renderItemIdentifier, resetStages}
		);
	}

	void LateStageVisibilityCheckStage::QueueRenderItemForStageEnable(
		const Entity::RenderItemIdentifier renderItemIdentifier, const Rendering::SceneRenderStageIdentifier stageIdentifier
	)
	{
		Threading::UniqueLock lock(m_mutex);
		m_queuedLateStageRenderItemChanges.EmplaceBack(QueuedLateStageRenderItemChange{Type::StageEnable, renderItemIdentifier, stageIdentifier}
		);
	}

	void LateStageVisibilityCheckStage::QueueRenderItemForStageDisable(
		const Entity::RenderItemIdentifier renderItemIdentifier, const Rendering::SceneRenderStageIdentifier stageIdentifier
	)
	{
		Threading::UniqueLock lock(m_mutex);
		m_queuedLateStageRenderItemChanges.EmplaceBack(
			QueuedLateStageRenderItemChange{Type::StageDisable, renderItemIdentifier, stageIdentifier}
		);
	}

	void LateStageVisibilityCheckStage::QueueRenderItemForStageReset(
		const Entity::RenderItemIdentifier renderItemIdentifier, const Rendering::SceneRenderStageIdentifier stageIdentifier
	)
	{
		Threading::UniqueLock lock(m_mutex);
		m_queuedLateStageRenderItemChanges.EmplaceBack(QueuedLateStageRenderItemChange{Type::StageReset, renderItemIdentifier, stageIdentifier}
		);
	}

	void LateStageVisibilityCheckStage::QueueActiveCameraPropertiesChanged()
	{
		// TODO: Can we use identifier storage for this?
		// Thread safe to get an identifier. Would nuke need for the lock
		// But would introduce need for knowing exactly how many late stage changes can happen in a frame.
		Threading::UniqueLock lock(m_mutex);
		m_queuedLateStageRenderItemChanges.EmplaceBack(QueuedLateStageRenderItemChange{Type::ActiveCameraPropertyChange});
	}

	void LateStageVisibilityCheckStage::QueueActiveCameraTransformChanged()
	{
		Threading::UniqueLock lock(m_mutex);
		m_queuedLateStageRenderItemChanges.EmplaceBack(QueuedLateStageRenderItemChange{Type::ActiveCameraTransformChange});
	}

	void LateStageVisibilityCheckStage::ClearQueue()
	{
		{
			Threading::UniqueLock lock(m_mutex);
			m_queuedLateStageRenderItemChanges.Clear();
		}

		m_removedRenderItems.Clear();
		m_disabledRenderItems.Clear();
	}
}
