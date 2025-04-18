#pragma once

#include <Engine/Entity/ComponentIdentifier.h>
#include <Engine/Entity/RenderItemIdentifier.h>
#include <Engine/Entity/RenderItemMask.h>

#include <Renderer/Stages/RenderItemStageMask.h>
#include <Renderer/Assets/Stage/SceneRenderStageIdentifier.h>
#include <Renderer/Scene/ViewMatrices.h>

#include <Common/Storage/IdentifierArray.h>
#include <Common/Function/ThreadSafeEvent.h>
#include <Common/Memory/UniqueRef.h>
#include <Common/Memory/Containers/Vector.h>

namespace ngine
{
	struct SceneBase;
	struct SceneViewModeBase;
}

namespace ngine::Entity
{
	struct HierarchyComponentBase;

	struct SceneRegistry;
}

namespace ngine::Rendering
{
	struct CommandEncoderView;
	struct PerFrameStagingBuffer;
	struct LogicalDevice;
	struct SceneRenderStage;
	struct SceneViewDrawer;
	struct RenderItemStage;
	struct RenderOutput;

	// TODO: Rename to SceneView. Existing SceneView -> Scene3DView, Scene3DData etc
	struct SceneViewBase
	{
		enum class Flags : uint8
		{
			IsEnabled = 1 << 0,
			HasScene = 1 << 1,
			IsSceneEnabled = 1 << 2,
			HasSceneFinishedLoading = 1 << 3,
			IsFramegraphReady = 1 << 4,
			HasCamera = 1 << 5,
			CanEnable = IsEnabled | HasScene | IsSceneEnabled | IsFramegraphReady
		};

		SceneViewBase(LogicalDevice& logicalDevice, SceneViewDrawer& drawer, RenderOutput& output, const EnumFlags<Flags> flags = {});
		virtual ~SceneViewBase();

		void OnCreated();

		[[nodiscard]] PURE_STATICS bool IsEnabled() const
		{
			return m_flags.AreAllSet(Flags::CanEnable);
		}
		[[nodiscard]] PURE_STATICS bool IsDisabled() const
		{
			return !IsEnabled();
		}
		void Disable();
		void Enable();
		ThreadSafe::Event<void(void*, ngine::SceneBase& scene), 24> OnEnabled;
		ThreadSafe::Event<void(void*, ngine::SceneBase& scene), 24> OnDisabled;

		void InvalidateFramegraph();
		void OnFramegraphBuilt();

		template<typename ModeType, typename... Args>
		ModeType& RegisterMode(Args&&... args)
		{
			ModeType& emplacedMode = static_cast<ModeType&>(*m_modes.EmplaceBack(UniqueRef<ModeType>::Make(Forward<Args>(args)...)));
			OnModeAdded(emplacedMode);
			return emplacedMode;
		}
		Event<void(void*, SceneViewModeBase&), 24> OnModeAdded;
		Event<void(void*, const SceneViewModeBase&), 24> OnModeRemoved;
		[[nodiscard]] Optional<SceneViewModeBase*> GetCurrentMode() const
		{
			return m_pCurrentMode;
		}
		[[nodiscard]] Optional<SceneViewModeBase*> FindMode(const Guid typeGuid) const;
		void ChangeMode(const Optional<SceneViewModeBase*> pMode);
		void ExitCurrentMode();

		void OnSceneFinishedLoading(SceneBase& base);

		[[nodiscard]] PURE_STATICS LogicalDevice& GetLogicalDevice()
		{
			return m_logicalDevice;
		}
		[[nodiscard]] PURE_STATICS const LogicalDevice& GetLogicalDevice() const
		{
			return m_logicalDevice;
		}

		[[nodiscard]] PURE_STATICS RenderOutput& GetOutput()
		{
			return m_output;
		}
		[[nodiscard]] PURE_STATICS const RenderOutput& GetOutput() const
		{
			return m_output;
		}

		[[nodiscard]] PURE_STATICS SceneViewDrawer& GetDrawer()
		{
			return m_drawer;
		}
		[[nodiscard]] PURE_STATICS const SceneViewDrawer& GetDrawer() const
		{
			return m_drawer;
		}

		[[nodiscard]] const ViewMatrices& GetMatrices() const
		{
			return m_viewMatrices;
		}

		void RegisterRenderItemStage(const SceneRenderStageIdentifier identifier, SceneRenderStage& stage)
		{
			RegisterSceneRenderStage(identifier, stage);
			m_renderItemStagesMask.Set(identifier);
		}
		void DeregisterRenderItemStage(const SceneRenderStageIdentifier identifier)
		{
			DeregisterSceneRenderStage(identifier);
			m_renderItemStagesMask.Clear(identifier);
		}

		void RegisterSceneRenderStage(const SceneRenderStageIdentifier identifier, SceneRenderStage& stage)
		{
			Assert(m_sceneRenderStages[identifier].IsInvalid());
			m_sceneRenderStages[identifier] = &stage;
		}
		void DeregisterSceneRenderStage(const SceneRenderStageIdentifier identifier)
		{
			m_sceneRenderStages[identifier] = nullptr;
		}
		using SceneRenderStagesArray = TIdentifierArray<Optional<SceneRenderStage*>, SceneRenderStageIdentifier>;
		using SceneRenderStagesView = SceneRenderStagesArray::ConstView;
		[[nodiscard]] PURE_STATICS Optional<SceneRenderStage*> GetSceneRenderStage(const SceneRenderStageIdentifier identifier) const
		{
			return m_sceneRenderStages[identifier];
		}
		[[nodiscard]] PURE_STATICS Optional<RenderItemStage*> GetRenderItemStage(const SceneRenderStageIdentifier identifier) const;

		void SetStageDependentOnCameraProperties(const SceneRenderStageIdentifier identifier)
		{
			m_cameraPropertyDependentStages.Set(identifier);
		}

		void StartTraversal();

		void ProcessRemovedRenderItems(SceneBase& scene, const Entity::RenderItemMask& renderItems);
		void ProcessDisabledRenderItems(SceneBase& scene, const Entity::RenderItemMask& renderItems);
		void
		ProcessDisabledRenderItemStageMask(const Entity::RenderItemIdentifier renderItemIdentifier, const RenderItemStageMask& resetStages);
		void ProcessEnabledRenderItemStage(
			const Entity::RenderItemIdentifier renderItemIdentifier, const Rendering::SceneRenderStageIdentifier stageIdentifier
		);
		void ProcessDisabledRenderItemStage(
			const Entity::RenderItemIdentifier renderItemIdentifier, const Rendering::SceneRenderStageIdentifier stageIdentifier
		);
		void ProcessResetRenderItemStage(
			const Entity::RenderItemIdentifier renderItemIdentifier, const Rendering::SceneRenderStageIdentifier stageIdentifier
		);
		void ProcessCameraPropertiesChanged(
			const Rendering::CommandEncoderView graphicsCommandEncoder, PerFrameStagingBuffer& perFrameStagingBuffer
		);
		void
		ProcessCameraTransformChanged(const Rendering::CommandEncoderView graphicsCommandEncoder, PerFrameStagingBuffer& perFrameStagingBuffer);

		void ProcessResetRenderItemStageMask(const Entity::RenderItemIdentifier renderItemIdentifier, const RenderItemStageMask& resetStages);
		void ProcessEnabledRenderItemStageMask(const Entity::RenderItemIdentifier renderItemIdentifier, const RenderItemStageMask& resetStages);
		void ProcessChangedRenderItemStageMask(
			const Entity::RenderItemIdentifier renderItemIdentifier,
			const RenderItemStageMask& enabledStages,
			const RenderItemStageMask& disabledStages
		);

		void DisableRenderItemStage(const SceneRenderStageIdentifier identifier);
		void EnableRenderItemStage(const SceneRenderStageIdentifier identifier);

		enum class TraversalResult : uint8
		{
			RemainedHidden,
			RemainedVisible,
			BecameVisible,
			BecameHidden
		};
		TraversalResult ProcessComponent(
			Entity::SceneRegistry& sceneRegistry,
			const Entity::ComponentIdentifier componentIdentifier,
			Entity::HierarchyComponentBase& component,
			const bool isVisible
		);

		void NotifyRenderStages(
			SceneBase& scene, const Rendering::CommandEncoderView graphicsCommandEncoder, PerFrameStagingBuffer& perFrameStagingBuffer
		);
		void OnTraversalFinished(SceneBase& scene);

		[[nodiscard]] PURE_STATICS Rendering::RenderItemStageMask& GetSubmittedRenderItemStageMask(const Entity::RenderItemIdentifier identifier
		)
		{
			return m_submittedRenderItemStageMasks[identifier];
		}
		[[nodiscard]] PURE_STATICS Rendering::RenderItemStageMask& GetQueuedRenderItemStageMask(const Entity::RenderItemIdentifier identifier)
		{
			return m_queuedRenderItemStageMasks[identifier];
		}

		[[nodiscard]] bool IsRenderItemVisible(const Entity::RenderItemIdentifier renderItemIdentifier) const
		{
			return m_visibleRenderItems.IsSet(renderItemIdentifier);
		}

		[[nodiscard]] PURE_STATICS Optional<Entity::HierarchyComponentBase*>
		GetVisibleRenderItemComponent(const Entity::RenderItemIdentifier renderItemIdentifier) const
		{
			return m_renderItemComponents[renderItemIdentifier];
		}
		[[nodiscard]] PURE_STATICS Entity::ComponentIdentifier
		GetVisibleRenderItemComponentIdentifier(const Entity::RenderItemIdentifier renderItemIdentifier) const
		{
			return Entity::ComponentIdentifier::MakeFromIndex(m_renderItemComponentIdentifiers[renderItemIdentifier]);
		}

		[[nodiscard]] FixedIdentifierArrayView<Entity::ComponentIdentifier::IndexType, Entity::RenderItemIdentifier>
		GetRenderItemComponentIdentifiers()
		{
			return m_renderItemComponentIdentifiers.GetView();
		}

		[[nodiscard]] Entity::RenderItemMask& GetVisibleRenderItems()
		{
			return m_visibleRenderItems;
		}
		[[nodiscard]] Entity::RenderItemMask& GetNewlyVisibleRenderItems()
		{
			return m_newlyVisibleRenderItems;
		}
		[[nodiscard]] Entity::RenderItemMask& GetNewlyHiddenRenderItems()
		{
			return m_newlyHiddenRenderItems;
		}
		[[nodiscard]] Entity::RenderItemMask& GetChangedTransformRenderItems()
		{
			return m_changedTransformRenderItems;
		}
	protected:
		void OnEnabledInternal();
		virtual void OnEnableFramegraph(SceneBase&, const Optional<Entity::HierarchyComponentBase*>)
		{
		}
		void OnDisabledInternal();
		virtual void OnDisableFramegraph(SceneBase&, const Optional<Entity::HierarchyComponentBase*>)
		{
		}
		void OnSceneAssigned(SceneBase& scene);
		void OnSceneDetached();
		void OnSceneEnabled();
		void OnSceneDisabled();
	protected:
		LogicalDevice& m_logicalDevice;
		SceneViewDrawer& m_drawer;
		RenderOutput& m_output;
		AtomicEnumFlags<Flags> m_flags;
		ViewMatrices m_viewMatrices;

		SceneViewModeBase* m_pCurrentMode = nullptr;
		Optional<uint32> m_previousModeIndex;
		Vector<UniqueRef<SceneViewModeBase>> m_modes;

		Optional<SceneBase*> m_pCurrentScene;
		Optional<Entity::HierarchyComponentBase*> m_pActiveCameraComponent;

		SceneRenderStagesArray m_sceneRenderStages{Memory::Zeroed};
		IdentifierMask<SceneRenderStageIdentifier> m_renderItemStagesMask;

		Entity::RenderItemMask m_newlyVisibleRenderItems;
		Entity::RenderItemMask m_newlyHiddenRenderItems;
		Entity::RenderItemMask m_changedTransformRenderItems;

		TIdentifierArray<Entity::RenderItemMask, SceneRenderStageIdentifier> m_newlyVisibleRenderStageItems{Memory::Zeroed};
		RenderItemStageMask m_newlyVisibleRenderStageItemsMask;
		TIdentifierArray<Entity::RenderItemMask, SceneRenderStageIdentifier> m_newlyHiddenRenderStageItems{Memory::Zeroed};
		RenderItemStageMask m_newlyHiddenRenderStageItemsMask;
		TIdentifierArray<Entity::RenderItemMask, SceneRenderStageIdentifier> m_changedTransformRenderStageItems{Memory::Zeroed};
		RenderItemStageMask m_changedTransformRenderStageItemsMask;
		TIdentifierArray<Entity::RenderItemMask, SceneRenderStageIdentifier> m_newlyResetRenderStageItems{Memory::Zeroed};
		RenderItemStageMask m_newlyResetRenderStageItemsMask;

		RenderItemStageMask m_newlyDisabledRenderItemStagesMask;
		RenderItemStageMask m_newlyEnabledRenderItemStagesMask;
		RenderItemStageMask m_cameraPropertyDependentStages;

		TIdentifierArray<RenderItemStageMask, Entity::RenderItemIdentifier> m_queuedRenderItemStageMasks{Memory::Zeroed};
		TIdentifierArray<RenderItemStageMask, Entity::RenderItemIdentifier> m_submittedRenderItemStageMasks{Memory::Zeroed};
		TIdentifierArray<Optional<Entity::HierarchyComponentBase*>, Entity::RenderItemIdentifier> m_renderItemComponents{Memory::Zeroed};
		TIdentifierArray<Entity::ComponentIdentifier::IndexType, Entity::RenderItemIdentifier> m_renderItemComponentIdentifiers{Memory::Zeroed};

		//! Entity::Data::RenderItem::TransformChangeTracker at time transform was uploaded to GPU
		TIdentifierArray<uint16, Entity::RenderItemIdentifier> m_submittedRenderItemTransformIds{Memory::Zeroed};

		Entity::RenderItemMask m_visibleRenderItemsBeforeTraversal;
		Entity::RenderItemMask m_visibleRenderItemsDuringTraversal;
		Entity::RenderItemMask m_visibleRenderItems;
	};

	ENUM_FLAG_OPERATORS(SceneViewBase::Flags);
}
