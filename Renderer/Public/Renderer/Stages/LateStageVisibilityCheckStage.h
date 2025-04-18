#pragma once

#include <Renderer/Constants.h>
#include <Renderer/Stages/Stage.h>
#include <Renderer/Stages/RenderItemStageMask.h>
#include <Renderer/Stages/PerFrameStagingBuffer.h>

#include <Engine/Entity/RenderItemIdentifier.h>

#include <Common/Storage/Identifier.h>
#include <Common/Storage/AtomicIdentifierMask.h>

#include <Common/Threading/Mutexes/Mutex.h>

namespace ngine::Entity
{
	struct HierarchyComponentBase;
}

namespace ngine::Rendering
{
	struct SceneView;
	struct CommandEncoderView;

	struct LateStageVisibilityCheckStage final : public Stage
	{
		LateStageVisibilityCheckStage(SceneView& sceneView);
		virtual ~LateStageVisibilityCheckStage();

		void QueueChangedRenderItemTransformForLateStageCulling(Entity::HierarchyComponentBase& renderItem);
		void QueueRenderItemForLateStageAdd(Entity::HierarchyComponentBase& renderItem);
		void QueueRenderItemForRemoval(const Entity::RenderItemIdentifier renderItemIdentifier);
		void QueueRenderItemEnabled(Entity::HierarchyComponentBase& renderItem);
		void QueueRenderItemDisabled(const Entity::RenderItemIdentifier renderItemIdentifier);
		void QueueRenderItemForStageMaskEnable(const Entity::RenderItemIdentifier renderItemIdentifier, const RenderItemStageMask& stages);
		void QueueRenderItemForStageMaskDisable(const Entity::RenderItemIdentifier renderItemIdentifier, const RenderItemStageMask& stages);
		void QueueRenderItemForStageMaskChanged(
			const Entity::RenderItemIdentifier renderItemIdentifier,
			const RenderItemStageMask& enabledStages,
			const RenderItemStageMask& disabledStages
		);
		void QueueRenderItemForStageMaskReset(const Entity::RenderItemIdentifier renderItemIdentifier, const RenderItemStageMask& resetStages);
		void QueueRenderItemForStageEnable(
			const Entity::RenderItemIdentifier renderItemIdentifier, const Rendering::SceneRenderStageIdentifier stageIdentifier
		);
		void QueueRenderItemForStageDisable(
			const Entity::RenderItemIdentifier renderItemIdentifier, const Rendering::SceneRenderStageIdentifier stageIdentifier
		);
		void QueueRenderItemForStageReset(
			const Entity::RenderItemIdentifier renderItemIdentifier, const Rendering::SceneRenderStageIdentifier stageIdentifier
		);
		void QueueActiveCameraPropertiesChanged();
		void QueueActiveCameraTransformChanged();

		void ClearQueue();
	protected:
		// Stage
		[[nodiscard]] virtual bool ShouldRecordCommands() const override;
		virtual void RecordCommands(const CommandEncoderView commandEncoder) override;

#if STAGE_DEPENDENCY_PROFILING
		[[nodiscard]] virtual ConstZeroTerminatedStringView GetDebugName() const override
		{
			return "Late Stage Visibility Check Stage";
		}
#endif

		[[nodiscard]] virtual EnumFlags<PipelineStageFlags> GetPipelineStageFlags() const override
		{
			return PipelineStageFlags::Transfer;
		}
		// ~Stage
	protected:
		enum class Type
		{
			//! Render item that is registered after active camera has been marked as finalized
			LateStageRegistration,
			//! Render item whose transform is changed after active camera has been marked as finalized
			LateStageTransformChange,
			RenderItemEnabled,
			StageMaskEnable,
			StageMaskDisable,
			StageMaskReset,
			StageMaskChanged,
			StageEnable,
			StageDisable,
			StageReset,
			ActiveCameraTransformChange,
			ActiveCameraPropertyChange,
		};

		struct QueuedLateStageRenderItemChange
		{
			QueuedLateStageRenderItemChange(const Type type)
				: m_type(type)
			{
				Assert(m_type == Type::ActiveCameraTransformChange || m_type == Type::ActiveCameraPropertyChange);
			}

			QueuedLateStageRenderItemChange(const Type type, Entity::HierarchyComponentBase& renderItem)
				: m_type(type)
			{
				Assert(m_type == Type::LateStageRegistration || m_type == Type::RenderItemEnabled || m_type == Type::LateStageTransformChange);
				m_renderItem = renderItem;
			}

			QueuedLateStageRenderItemChange(
				const Type type, const Entity::RenderItemIdentifier renderItemIdentifier, const RenderItemStageMask& stages
			)
				: m_type(type)
			{
				Assert(type == Type::StageMaskReset || type == Type::StageMaskEnable || type == Type::StageMaskDisable);
				m_stageMaskInfo.m_renderItemIdentifier = renderItemIdentifier;
				m_stageMaskInfo.m_stages = stages;
			}
			QueuedLateStageRenderItemChange(
				const Type type,
				const Entity::RenderItemIdentifier renderItemIdentifier,
				const RenderItemStageMask& enabledStages,
				const RenderItemStageMask& disabledStages
			)
				: m_type(type)
			{
				Assert(type == Type::StageMaskChanged);
				m_changedStagemaskInfo.m_renderItemIdentifier = renderItemIdentifier;
				m_changedStagemaskInfo.m_enabledStages = enabledStages;
				m_changedStagemaskInfo.m_disabledStages = disabledStages;
			}
			QueuedLateStageRenderItemChange(
				const Type type,
				const Entity::RenderItemIdentifier renderItemIdentifier,
				const Rendering::SceneRenderStageIdentifier stageIdentifier
			)
				: m_type(type)
			{
				Assert(type == Type::StageEnable || type == Type::StageDisable || type == Type::StageReset);
				m_stageInfo.m_renderItemIdentifier = renderItemIdentifier;
				m_stageInfo.m_stageIdentifier = stageIdentifier;
			}

			QueuedLateStageRenderItemChange(QueuedLateStageRenderItemChange&& other)
				: m_type(other.m_type)
			{
				Memory::CopyNonOverlappingElement(*this, other);
				/*switch (m_type)
				{
				case Type::Removal:
				  m_renderItemIdentifier = other.m_renderItemIdentifier;
				  break;
				default:
				  m_renderItem = other.m_renderItem;
				  break;
				}*/
			}

			Type m_type;
			union
			{
				ReferenceWrapper<Entity::HierarchyComponentBase> m_renderItem;
				Entity::RenderItemIdentifier m_renderItemIdentifier;
				struct
				{
					Entity::RenderItemIdentifier m_renderItemIdentifier;
					RenderItemStageMask m_stages;
				} m_stageMaskInfo;
				struct
				{
					Entity::RenderItemIdentifier m_renderItemIdentifier;
					RenderItemStageMask m_enabledStages;
					RenderItemStageMask m_disabledStages;
				} m_changedStagemaskInfo;
				struct
				{
					Entity::RenderItemIdentifier m_renderItemIdentifier;
					Rendering::SceneRenderStageIdentifier m_stageIdentifier;
				} m_stageInfo;
			};
		};

		SceneView& m_sceneView;
		Vector<QueuedLateStageRenderItemChange> m_queuedLateStageRenderItemChanges;
		Vector<QueuedLateStageRenderItemChange> m_queuedLateStageRenderItemChangesCopy;
		Threading::Mutex m_mutex;
		Threading::AtomicIdentifierMask<Entity::RenderItemIdentifier> m_removedRenderItems;
		Threading::AtomicIdentifierMask<Entity::RenderItemIdentifier> m_disabledRenderItems;

		PerFrameStagingBuffer m_perFrameStagingBuffer;
	};
}
