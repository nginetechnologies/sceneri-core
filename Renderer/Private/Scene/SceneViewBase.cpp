#include "Scene/SceneViewBase.h"

#include "Stages/RenderItemStage.h"

#include <Engine/Entity/Scene/SceneRegistry.h>
#include <Engine/Entity/HierarchyComponentBase.h>
#include <Engine/Entity/ComponentSoftReference.inl>
#include <Engine/Entity/ComponentTypeSceneData.h>
#include <Engine/Entity/Data/RenderItem/StageMask.h>
#include <Engine/Entity/Data/RenderItem/TransformChangeTracker.h>
#include <Engine/Entity/Data/RenderItem/Identifier.h>
#include <Engine/Scene/SceneBase.h>

#include <Renderer/Renderer.h>
#include <Renderer/Devices/LogicalDevice.h>
#include <Renderer/Scene/SceneViewDrawer.h>
#include <Renderer/Scene/SceneViewModeBase.h>
#include <Renderer/Framegraph/Framegraph.h>

namespace ngine::Rendering
{
	SceneViewBase::SceneViewBase(LogicalDevice& logicalDevice, SceneViewDrawer& drawer, RenderOutput& output, const EnumFlags<Flags> flags)
		: m_logicalDevice(logicalDevice)
		, m_drawer(drawer)
		, m_output(output)
		, m_flags(flags & Flags::IsEnabled)
		, m_viewMatrices(logicalDevice)
	{
	}

	SceneViewBase::~SceneViewBase()
	{
		m_viewMatrices.Destroy(m_logicalDevice);
	}

	void SceneViewBase::OnCreated()
	{
		if (m_flags.AreAllSet(Flags::CanEnable))
		{
			OnEnabledInternal();
		}
	}

	void SceneViewBase::Enable()
	{
		const EnumFlags<Flags> previousFlags = m_flags.FetchOr(Flags::IsEnabled);
		if ((!previousFlags.IsSet(Flags::IsEnabled) && (previousFlags | Flags::IsEnabled).AreAllSet(Flags::CanEnable)))
		{
			OnEnabledInternal();
		}
	}

	void SceneViewBase::OnEnabledInternal()
	{
		Assert(m_flags.AreAllSet(Flags::CanEnable));

		SceneBase& scene = *m_pCurrentScene;
		Entity::SceneRegistry& sceneRegistry = scene.GetEntitySceneRegistry();
		Entity::ComponentSoftReference cameraReference;
		Entity::HierarchyComponentBase* pCameraComponent = m_pActiveCameraComponent;
		if (pCameraComponent != nullptr)
		{
			cameraReference = Entity::ComponentSoftReference(*pCameraComponent, sceneRegistry);
		}

		Assert(scene.IsEnabled());
		scene.ModifyFrameGraph(
			[this, &scene, cameraReference]()
			{
				OnEnableFramegraph(scene, cameraReference.Find<Entity::HierarchyComponentBase>(scene.GetEntitySceneRegistry()));
				OnEnabled(scene);
			}
		);
	}

	void SceneViewBase::Disable()
	{
		const EnumFlags<Flags> previousFlags = m_flags.FetchAnd(~Flags::IsEnabled);
		if (previousFlags.AreAllSet(Flags::CanEnable))
		{
			OnDisabledInternal();
		}
	}

	void SceneViewBase::OnDisabledInternal()
	{
		Assert(!m_flags.AreAllSet(Flags::CanEnable));

		SceneBase& scene = *m_pCurrentScene;
		Entity::SceneRegistry& sceneRegistry = scene.GetEntitySceneRegistry();
		Entity::ComponentSoftReference cameraReference;
		Entity::HierarchyComponentBase* pCameraComponent = m_pActiveCameraComponent;
		if (pCameraComponent != nullptr)
		{
			cameraReference = Entity::ComponentSoftReference(*pCameraComponent, sceneRegistry);
		}

		scene.ModifyFrameGraph(
			[this, &scene, cameraReference]()
			{
				OnDisableFramegraph(scene, cameraReference.Find<Entity::HierarchyComponentBase>(scene.GetEntitySceneRegistry()));
				OnDisabled(scene);
			}
		);
	}

	void SceneViewBase::OnSceneAssigned(SceneBase& scene)
	{
		Assert(!IsEnabled());

		if (m_pCurrentMode != nullptr && m_pCurrentScene != nullptr)
		{
			m_pCurrentMode->OnSceneUnloading(*m_pCurrentScene, *this);
		}

		Assert(m_pCurrentScene != &scene);
		m_pCurrentScene = &scene;

		scene.OnEnabledUpdate.Add(
			*this,
			[](SceneViewBase& sceneView)
			{
				const Optional<SceneBase*> pScene = sceneView.m_pCurrentScene;
				Assert(pScene.IsValid() && pScene->IsEnabled());
				if (pScene.IsValid() && pScene->IsEnabled())
				{
					sceneView.OnSceneEnabled();
				}
			}
		);
		scene.OnDisabledUpdate.Add(
			*this,
			[](SceneViewBase& sceneView)
			{
				const Optional<SceneBase*> pScene = sceneView.m_pCurrentScene;
				Assert(pScene.IsInvalid() || pScene->IsDisabled());
				if (pScene.IsInvalid() || pScene->IsDisabled())
				{
					sceneView.OnSceneDisabled();
				}
			}
		);

		Assert(m_flags.IsNotSet(Flags::HasScene));
		Assert(m_flags.IsNotSet(Flags::HasSceneFinishedLoading));
		Assert(m_flags.IsNotSet(Flags::HasCamera));
		Assert(m_flags.IsNotSet(Flags::IsSceneEnabled));
		const EnumFlags<Flags> newFlags = Flags::HasScene | (Flags::IsSceneEnabled * scene.IsEnabled());
		const EnumFlags<Flags> previousFlags = m_flags.FetchOr(newFlags);
		if ((previousFlags | newFlags).AreAllSet(Flags::CanEnable) && !previousFlags.AreAllSet(Flags::CanEnable))
		{
			OnEnabledInternal();
		}

		if (m_pCurrentMode != nullptr)
		{
			m_pCurrentMode->OnSceneAssigned(scene, *this);
		}

		scene.AssignSceneView(*this);
	}

	void SceneViewBase::OnSceneDetached()
	{
		if (m_pCurrentScene != nullptr)
		{
			SceneBase& scene = *m_pCurrentScene;
			if (m_pCurrentMode != nullptr)
			{
				m_pCurrentMode->OnSceneUnloading(scene, *this);
			}

			scene.OnEnabledUpdate.Remove(this);
			scene.OnDisabledUpdate.Remove(this);

			scene.OnSceneViewRemoved(*this);
		}

		const EnumFlags<Flags> previousFlags = m_flags.FetchAnd(~(Flags::HasScene | Flags::HasSceneFinishedLoading | Flags::IsSceneEnabled));
		Assert(previousFlags.IsSet(Flags::HasScene));
		if (previousFlags.AreAllSet(Flags::CanEnable))
		{
			OnDisabledInternal();
		}

		m_newlyVisibleRenderItems.ClearAll();
		m_newlyHiddenRenderItems.ClearAll();
		m_changedTransformRenderItems.ClearAll();

		m_newlyVisibleRenderStageItems.GetView().ZeroInitialize();
		m_newlyVisibleRenderStageItemsMask.ClearAll();
		m_newlyHiddenRenderStageItems.GetView().ZeroInitialize();
		m_newlyHiddenRenderStageItemsMask.ClearAll();
		m_changedTransformRenderStageItems.GetView().ZeroInitialize();
		m_changedTransformRenderStageItemsMask.ClearAll();
		m_newlyResetRenderStageItems.GetView().ZeroInitialize();
		m_newlyResetRenderStageItemsMask.ClearAll();

		m_newlyDisabledRenderItemStagesMask.ClearAll();
		m_newlyEnabledRenderItemStagesMask.ClearAll();
		m_cameraPropertyDependentStages.ClearAll();

		m_renderItemComponents.GetView().ZeroInitialize();
		m_renderItemComponentIdentifiers.GetView().ZeroInitialize();
		m_queuedRenderItemStageMasks.GetView().ZeroInitialize();
		m_submittedRenderItemStageMasks.GetView().ZeroInitialize();
		m_submittedRenderItemTransformIds.GetView().ZeroInitialize();

		m_visibleRenderItemsBeforeTraversal.ClearAll();
		m_visibleRenderItemsDuringTraversal.ClearAll();
		m_visibleRenderItems.ClearAll();

		m_logicalDevice.GetRenderer().GetStageCache().IterateElements(
			m_sceneRenderStages.GetView(),
			[](Optional<SceneRenderStage*>& stage)
			{
				if (stage.IsValid())
				{
					stage->OnSceneUnloaded();
				}
			}
		);

		m_pCurrentScene = nullptr;
	}

	void SceneViewBase::OnSceneEnabled()
	{
		Assert(m_pCurrentScene.IsValid() && m_pCurrentScene->IsEnabled());
		const EnumFlags<Flags> previousFlags = m_flags.FetchOr(Flags::IsSceneEnabled);
		if (!previousFlags.IsSet(Flags::IsSceneEnabled) && (previousFlags | Flags::IsSceneEnabled).AreAllSet(Flags::CanEnable))
		{
			OnEnabledInternal();
		}
	}

	void SceneViewBase::OnSceneDisabled()
	{
		Assert(m_pCurrentScene.IsInvalid() || m_pCurrentScene->IsDisabled());
		const EnumFlags<Flags> previousFlags = m_flags.FetchAnd(~Flags::IsSceneEnabled);
		if (previousFlags.AreAllSet(Flags::CanEnable))
		{
			OnDisabledInternal();
		}
	}

	void SceneViewBase::InvalidateFramegraph()
	{
		const EnumFlags<Flags> previousFlags = m_flags.FetchAnd(~Flags::IsFramegraphReady);
		if (previousFlags.AreAllSet(Flags::CanEnable))
		{
			OnDisabledInternal();
		}
	}

	void SceneViewBase::OnFramegraphBuilt()
	{
		const EnumFlags<Flags> previousFlags = m_flags.FetchOr(Flags::IsFramegraphReady);
		if ((!previousFlags.IsSet(Flags::IsFramegraphReady) && (previousFlags | Flags::IsFramegraphReady).AreAllSet(Flags::CanEnable)))
		{
			OnEnabledInternal();
		}
	}

	void SceneViewBase::ProcessRemovedRenderItems(SceneBase& scene, const Entity::RenderItemMask& renderItems)
	{
		const Entity::RenderItemMask visibleRenderItems = m_visibleRenderItems & renderItems;

		const typename Entity::RenderItemIdentifier::IndexType maximumUsedRenderItemCount = scene.GetMaximumUsedRenderItemCount();
		for (const uint32 renderItemIndex : visibleRenderItems.GetSetBitsIterator(0, maximumUsedRenderItemCount))
		{
			const Entity::RenderItemIdentifier renderItemIdentifier = Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex);

			m_newlyVisibleRenderItems.Clear(renderItemIdentifier);
			m_newlyHiddenRenderItems.Set(renderItemIdentifier);

			const RenderItemStageMask& queuedStageMask = m_queuedRenderItemStageMasks[renderItemIdentifier];
			for (const typename RenderItemStageMask::BitIndexType stageIndex : queuedStageMask.GetSetBitsIterator())
			{
				const SceneRenderStageIdentifier stageIdentifier = SceneRenderStageIdentifier::MakeFromValidIndex(stageIndex);
				m_newlyVisibleRenderStageItems[stageIdentifier].Clear(renderItemIdentifier);
				m_newlyHiddenRenderStageItems[stageIdentifier].Set(renderItemIdentifier);
			}

			m_newlyHiddenRenderStageItemsMask |= queuedStageMask;
		}

		for (const uint32 renderItemIndex : visibleRenderItems.GetSetBitsIterator(0, maximumUsedRenderItemCount))
		{
			const Entity::RenderItemIdentifier renderItemIdentifier = Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex);
			m_renderItemComponents[renderItemIdentifier] = Invalid;
			m_renderItemComponentIdentifiers[renderItemIdentifier] = Entity::ComponentIdentifier::Invalid;
			m_queuedRenderItemStageMasks[renderItemIdentifier].ClearAll();
		}

		m_visibleRenderItems.Clear(renderItems);
		m_visibleRenderItemsDuringTraversal.Clear(renderItems);

		for (const uint32 renderItemIndex : renderItems.GetSetBitsIterator(0, maximumUsedRenderItemCount))
		{
			scene.ReturnRenderItemIdentifier(Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex));
		}
	}

	void SceneViewBase::ProcessDisabledRenderItems(SceneBase& scene, const Entity::RenderItemMask& renderItems)
	{
		const Entity::RenderItemMask visibleRenderItems = m_visibleRenderItems & renderItems;

		const typename Entity::RenderItemIdentifier::IndexType maximumUsedRenderItemCount = scene.GetMaximumUsedRenderItemCount();
		for (const uint32 renderItemIndex : visibleRenderItems.GetSetBitsIterator(0, maximumUsedRenderItemCount))
		{
			const Entity::RenderItemIdentifier renderItemIdentifier = Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex);

			const RenderItemStageMask& queuedStageMask = m_queuedRenderItemStageMasks[renderItemIdentifier];
			for (const typename RenderItemStageMask::BitIndexType stageIndex : queuedStageMask.GetSetBitsIterator())
			{
				const SceneRenderStageIdentifier stageIdentifier = SceneRenderStageIdentifier::MakeFromValidIndex(stageIndex);
				m_newlyVisibleRenderStageItems[stageIdentifier].Clear(renderItemIdentifier);
				m_newlyHiddenRenderStageItems[stageIdentifier].Set(renderItemIdentifier);
			}

			m_newlyHiddenRenderStageItemsMask |= queuedStageMask;
		}

		for (const uint32 renderItemIndex : visibleRenderItems.GetSetBitsIterator(0, maximumUsedRenderItemCount))
		{
			const Entity::RenderItemIdentifier renderItemIdentifier = Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex);
			m_renderItemComponents[renderItemIdentifier] = Invalid;
			m_renderItemComponentIdentifiers[renderItemIdentifier] = Entity::ComponentIdentifier::Invalid;
			m_queuedRenderItemStageMasks[renderItemIdentifier].ClearAll();
		}

		m_visibleRenderItems.Clear(renderItems);
		m_visibleRenderItemsDuringTraversal.Clear(renderItems);
		m_changedTransformRenderItems.Clear(renderItems);
		m_newlyVisibleRenderItems.Clear(renderItems);
		m_newlyHiddenRenderItems |= renderItems;
	}

	void SceneViewBase::ProcessResetRenderItemStageMask(
		const Entity::RenderItemIdentifier renderItemIdentifier, const RenderItemStageMask& resetStages
	)
	{
		if (!m_visibleRenderItems.IsSet(renderItemIdentifier))
		{
			return;
		}

		const RenderItemStageMask& queuedStageMask = m_queuedRenderItemStageMasks[renderItemIdentifier];
		const RenderItemStageMask changedStagesMask = queuedStageMask & resetStages;
		m_newlyResetRenderStageItemsMask |= changedStagesMask;

		for (const typename RenderItemStageMask::BitIndexType stageIndex : changedStagesMask.GetSetBitsIterator())
		{
			const SceneRenderStageIdentifier stageIdentifier = SceneRenderStageIdentifier::MakeFromValidIndex(stageIndex);
			Assert(!m_newlyHiddenRenderStageItems[stageIdentifier].IsSet(renderItemIdentifier));
			m_newlyResetRenderStageItems[stageIdentifier].Set(renderItemIdentifier);
		}
	}

	void SceneViewBase::ProcessEnabledRenderItemStageMask(
		const Entity::RenderItemIdentifier renderItemIdentifier, const RenderItemStageMask& enabledStages
	)
	{
		if (!m_visibleRenderItems.IsSet(renderItemIdentifier))
		{
			return;
		}

		RenderItemStageMask& queuedStageMask = m_queuedRenderItemStageMasks[renderItemIdentifier];
		const RenderItemStageMask changedStagesMask = (queuedStageMask ^ enabledStages) & enabledStages;
		m_newlyVisibleRenderStageItemsMask |= changedStagesMask;
		queuedStageMask |= changedStagesMask;

		for (const typename RenderItemStageMask::BitIndexType stageIndex : changedStagesMask.GetSetBitsIterator())
		{
			const SceneRenderStageIdentifier stageIdentifier = SceneRenderStageIdentifier::MakeFromValidIndex(stageIndex);
			m_newlyHiddenRenderStageItems[stageIdentifier].Clear(renderItemIdentifier);

			Assert(queuedStageMask.IsSet(stageIdentifier));
			m_newlyVisibleRenderStageItems[stageIdentifier].Set(renderItemIdentifier);
		}
	}

	void SceneViewBase::ProcessDisabledRenderItemStageMask(
		const Entity::RenderItemIdentifier renderItemIdentifier, const RenderItemStageMask& disabledStages
	)
	{
		if (!m_visibleRenderItems.IsSet(renderItemIdentifier))
		{
			return;
		}

		RenderItemStageMask& queuedStageMask = m_queuedRenderItemStageMasks[renderItemIdentifier];
		const RenderItemStageMask changedStagesMask = queuedStageMask & ~disabledStages;

		m_newlyHiddenRenderStageItemsMask |= changedStagesMask;
		queuedStageMask &= ~changedStagesMask;

		for (const typename RenderItemStageMask::BitIndexType stageIndex : changedStagesMask.GetSetBitsIterator())
		{
			const SceneRenderStageIdentifier stageIdentifier = SceneRenderStageIdentifier::MakeFromValidIndex(stageIndex);
			m_newlyVisibleRenderStageItems[stageIdentifier].Clear(renderItemIdentifier);
			m_newlyHiddenRenderStageItems[stageIdentifier].Set(renderItemIdentifier);
		}
	}

	void SceneViewBase::ProcessChangedRenderItemStageMask(
		const Entity::RenderItemIdentifier renderItemIdentifier,
		const RenderItemStageMask& __restrict enabledStages,
		const RenderItemStageMask& __restrict disabledStages
	)
	{
		if (!m_visibleRenderItems.IsSet(renderItemIdentifier))
		{
			return;
		}

		RenderItemStageMask& queuedStageMask = m_queuedRenderItemStageMasks[renderItemIdentifier];
		{
			const RenderItemStageMask changedStagesMask = (queuedStageMask ^ enabledStages) & enabledStages;
			m_newlyVisibleRenderStageItemsMask |= changedStagesMask;
			queuedStageMask |= changedStagesMask;
			for (const typename RenderItemStageMask::BitIndexType stageIndex : changedStagesMask.GetSetBitsIterator())
			{
				const SceneRenderStageIdentifier stageIdentifier = SceneRenderStageIdentifier::MakeFromValidIndex(stageIndex);
				m_newlyHiddenRenderStageItems[stageIdentifier].Clear(renderItemIdentifier);

				Assert(queuedStageMask.IsSet(stageIdentifier));
				m_newlyVisibleRenderStageItems[stageIdentifier].Set(renderItemIdentifier);
			}
		}

		{
			const RenderItemStageMask changedStagesMask = queuedStageMask & ~disabledStages;
			m_newlyHiddenRenderStageItemsMask |= changedStagesMask;
			queuedStageMask &= ~changedStagesMask;
			for (const typename RenderItemStageMask::BitIndexType stageIndex : changedStagesMask.GetSetBitsIterator())
			{
				const SceneRenderStageIdentifier stageIdentifier = SceneRenderStageIdentifier::MakeFromValidIndex(stageIndex);
				m_newlyVisibleRenderStageItems[stageIdentifier].Clear(renderItemIdentifier);
				m_newlyHiddenRenderStageItems[stageIdentifier].Set(renderItemIdentifier);
			}
		}
	}

	void SceneViewBase::ProcessEnabledRenderItemStage(
		const Entity::RenderItemIdentifier renderItemIdentifier, const Rendering::SceneRenderStageIdentifier stageIdentifier
	)
	{
		if (!m_visibleRenderItems.IsSet(renderItemIdentifier))
		{
			return;
		}

		RenderItemStageMask& queuedStageMask = m_queuedRenderItemStageMasks[renderItemIdentifier];
		if (!queuedStageMask.IsSet(stageIdentifier))
		{
			m_newlyHiddenRenderStageItems[stageIdentifier].Clear(renderItemIdentifier);
			m_newlyVisibleRenderStageItemsMask.Set(stageIdentifier);
			m_newlyVisibleRenderStageItems[stageIdentifier].Set(renderItemIdentifier);
			queuedStageMask.Set(stageIdentifier);
		}
	}

	void SceneViewBase::ProcessDisabledRenderItemStage(
		const Entity::RenderItemIdentifier renderItemIdentifier, const Rendering::SceneRenderStageIdentifier stageIdentifier
	)
	{
		if (!m_visibleRenderItems.IsSet(renderItemIdentifier))
		{
			return;
		}

		RenderItemStageMask& queuedStageMask = m_queuedRenderItemStageMasks[renderItemIdentifier];
		if (queuedStageMask.IsSet(stageIdentifier))
		{
			m_newlyVisibleRenderStageItems[stageIdentifier].Clear(renderItemIdentifier);
			m_newlyHiddenRenderStageItemsMask.Set(stageIdentifier);
			m_newlyHiddenRenderStageItems[stageIdentifier].Set(renderItemIdentifier);
			queuedStageMask.Clear(stageIdentifier);
		}
	}

	void SceneViewBase::ProcessResetRenderItemStage(
		const Entity::RenderItemIdentifier renderItemIdentifier, const Rendering::SceneRenderStageIdentifier stageIdentifier
	)
	{
		if (!m_visibleRenderItems.IsSet(renderItemIdentifier))
		{
			return;
		}

		RenderItemStageMask& queuedStageMask = m_queuedRenderItemStageMasks[renderItemIdentifier];
		if (queuedStageMask.IsSet(stageIdentifier))
		{
			m_newlyResetRenderStageItemsMask.Set(stageIdentifier);
			m_newlyResetRenderStageItems[stageIdentifier].Set(renderItemIdentifier);

			Assert(!m_newlyVisibleRenderStageItems[stageIdentifier].IsSet(renderItemIdentifier));
			Assert(!m_newlyHiddenRenderStageItems[stageIdentifier].IsSet(renderItemIdentifier));
		}
	}

	void SceneViewBase::StartTraversal()
	{
		m_visibleRenderItemsBeforeTraversal = m_visibleRenderItems;
		m_visibleRenderItemsDuringTraversal.ClearAll();
	}

	void SceneViewBase::DisableRenderItemStage(const SceneRenderStageIdentifier identifier)
	{
		m_newlyDisabledRenderItemStagesMask.Set(identifier);

		if (const Optional<RenderItemStage*> pStage = GetRenderItemStage(identifier))
		{
			pStage->Disable();
		}
	}

	void SceneViewBase::EnableRenderItemStage(const SceneRenderStageIdentifier identifier)
	{
		m_newlyEnabledRenderItemStagesMask.Set(identifier);

		if (const Optional<RenderItemStage*> pStage = GetRenderItemStage(identifier))
		{
			pStage->Enable();
		}
	}

	SceneViewBase::TraversalResult SceneViewBase::ProcessComponent(
		Entity::SceneRegistry& sceneRegistry,
		const Entity::ComponentIdentifier componentIdentifier,
		Entity::HierarchyComponentBase& component,
		const bool isVisible
	)
	{
		const AtomicRenderItemStageMask& __restrict renderItemStageMask =
			sceneRegistry.GetCachedSceneData<Entity::Data::RenderItem::StageMask>().GetComponentImplementationUnchecked(componentIdentifier);
		const Entity::RenderItemIdentifier renderItemIdentifier =
			sceneRegistry.GetCachedSceneData<Entity::Data::RenderItem::Identifier>().GetComponentImplementationUnchecked(componentIdentifier);

		const RenderItemStageMask stageMask = reinterpret_cast<const RenderItemStageMask&>(renderItemStageMask);
		const bool wasVisible = m_visibleRenderItems.IsSet(renderItemIdentifier);
		if (wasVisible == isVisible)
		{
			if (isVisible)
			{
				m_visibleRenderItemsDuringTraversal.Set(renderItemIdentifier);

				RenderItemStageMask& queuedStageMask = m_queuedRenderItemStageMasks[renderItemIdentifier];
				const RenderItemStageMask changedMask = stageMask ^ queuedStageMask;
				const RenderItemStageMask enabledStages = (stageMask & changedMask);
				const RenderItemStageMask disabledStages = ~enabledStages & changedMask;
				queuedStageMask = stageMask;

				for (const typename RenderItemStageMask::BitIndexType stageIndex : enabledStages.GetSetBitsIterator())
				{
					const SceneRenderStageIdentifier stageIdentifier = SceneRenderStageIdentifier::MakeFromValidIndex(stageIndex);
					Assert(queuedStageMask.IsSet(stageIdentifier));
					m_newlyHiddenRenderStageItems[stageIdentifier].Clear(renderItemIdentifier);
					m_newlyVisibleRenderStageItems[stageIdentifier].Set(renderItemIdentifier);
				}
				m_newlyVisibleRenderStageItemsMask |= enabledStages;

				for (const typename RenderItemStageMask::BitIndexType stageIndex : disabledStages.GetSetBitsIterator())
				{
					const SceneRenderStageIdentifier stageIdentifier = SceneRenderStageIdentifier::MakeFromValidIndex(stageIndex);
					m_newlyVisibleRenderStageItems[stageIdentifier].Clear(renderItemIdentifier);
					m_newlyHiddenRenderStageItems[stageIdentifier].Set(renderItemIdentifier);
				}
				m_newlyHiddenRenderStageItemsMask |= disabledStages;

				using TransformChangeTracker = Entity::Data::RenderItem::TransformChangeTracker;
				const TransformChangeTracker& __restrict transformChangeTracker =
					sceneRegistry.GetCachedSceneData<Entity::Data::RenderItem::TransformChangeTracker>().GetComponentImplementationUnchecked(
						componentIdentifier
					);

				TransformChangeTracker::ValueType& submittedTransformId = m_submittedRenderItemTransformIds[renderItemIdentifier];
				if (transformChangeTracker != submittedTransformId)
				{
					Assert(m_renderItemComponents[renderItemIdentifier].IsValid());
					m_changedTransformRenderItems.Set(renderItemIdentifier);

					for (const typename RenderItemStageMask::BitIndexType stageIndex : stageMask.GetSetBitsIterator())
					{
						const SceneRenderStageIdentifier stageIdentifier = SceneRenderStageIdentifier::MakeFromValidIndex(stageIndex);
						m_changedTransformRenderStageItems[stageIdentifier].Set(renderItemIdentifier);
					}

					m_changedTransformRenderStageItemsMask |= stageMask;
					submittedTransformId = transformChangeTracker.GetValue();
				}
				return TraversalResult::RemainedVisible;
			}
			else
			{
				return TraversalResult::RemainedHidden;
			}
		}

		if (isVisible)
		{
			Assert(!m_visibleRenderItems.IsSet(renderItemIdentifier));
			Assert(!m_visibleRenderItemsDuringTraversal.IsSet(renderItemIdentifier));

			m_visibleRenderItems.Set(renderItemIdentifier);
			m_visibleRenderItemsDuringTraversal.Set(renderItemIdentifier);

			m_queuedRenderItemStageMasks[renderItemIdentifier] = stageMask;
			m_renderItemComponents[renderItemIdentifier] = &component;
			m_renderItemComponentIdentifiers[renderItemIdentifier] = componentIdentifier.GetIndex();

			m_newlyHiddenRenderItems.Clear(renderItemIdentifier);
			m_newlyVisibleRenderItems.Set(renderItemIdentifier);

			for (const typename RenderItemStageMask::BitIndexType stageIndex : stageMask.GetSetBitsIterator())
			{
				const SceneRenderStageIdentifier stageIdentifier = SceneRenderStageIdentifier::MakeFromValidIndex(stageIndex);
				m_newlyHiddenRenderStageItems[stageIdentifier].Clear(renderItemIdentifier);
				Assert(m_queuedRenderItemStageMasks[renderItemIdentifier].IsSet(stageIdentifier));
				m_newlyVisibleRenderStageItems[stageIdentifier].Set(renderItemIdentifier);
			}
			m_newlyVisibleRenderStageItemsMask |= stageMask;
			return TraversalResult::BecameVisible;
		}
		else
		{
			Assert(m_visibleRenderItems.IsSet(renderItemIdentifier));

			m_visibleRenderItems.Clear(renderItemIdentifier);
			m_visibleRenderItemsDuringTraversal.Clear(renderItemIdentifier);
			m_renderItemComponents[renderItemIdentifier] = Invalid;

			m_newlyVisibleRenderItems.Clear(renderItemIdentifier);
			m_newlyHiddenRenderItems.Set(renderItemIdentifier);

			RenderItemStageMask& queuedStageMask = m_queuedRenderItemStageMasks[renderItemIdentifier];
			for (const typename RenderItemStageMask::BitIndexType stageIndex : queuedStageMask.GetSetBitsIterator())
			{
				const SceneRenderStageIdentifier stageIdentifier = SceneRenderStageIdentifier::MakeFromValidIndex(stageIndex);
				m_newlyVisibleRenderStageItems[stageIdentifier].Clear(renderItemIdentifier);
				m_newlyHiddenRenderStageItems[stageIdentifier].Set(renderItemIdentifier);
			}
			m_newlyHiddenRenderStageItemsMask |= queuedStageMask;
			queuedStageMask.ClearAll();
			return TraversalResult::BecameHidden;
		}
	}

	void SceneViewBase::NotifyRenderStages(
		SceneBase& scene, const Rendering::CommandEncoderView graphicsCommandEncoder, PerFrameStagingBuffer& perFrameStagingBuffer
	)
	{
		const typename Entity::RenderItemIdentifier::IndexType maximumUsedRenderItemCount = scene.GetMaximumUsedRenderItemCount();

		if (m_newlyVisibleRenderItems.AreAnySet())
		{
			m_newlyVisibleRenderItems.ClearAll();
		}

		if (m_changedTransformRenderItems.AreAnySet())
		{
			m_changedTransformRenderItems.ClearAll();
		}

		if (m_newlyHiddenRenderItems.AreAnySet())
		{
			// m_transformBuffer.OnRenderItemsBecomeHidden(m_newlyHiddenRenderItems, maximumUsedRenderItemCount, m_logicalDevice);
			m_newlyHiddenRenderItems.ClearAll();
		}

		if (m_newlyDisabledRenderItemStagesMask.AreAnySet())
		{
			for (const typename RenderItemStageMask::BitIndexType stageIndex : m_newlyDisabledRenderItemStagesMask.GetSetBitsIterator())
			{
				const SceneRenderStageIdentifier stageIdentifier = SceneRenderStageIdentifier::MakeFromValidIndex(stageIndex);
				if (const Optional<RenderItemStage*> pStage = GetRenderItemStage(stageIdentifier))
				{
					pStage->OnDisabled(graphicsCommandEncoder, perFrameStagingBuffer);
				}
			}
		}
		m_newlyDisabledRenderItemStagesMask.ClearAll();

		if (m_newlyEnabledRenderItemStagesMask.AreAnySet())
		{
			for (const typename RenderItemStageMask::BitIndexType stageIndex : m_newlyEnabledRenderItemStagesMask.GetSetBitsIterator())
			{
				const SceneRenderStageIdentifier stageIdentifier = SceneRenderStageIdentifier::MakeFromValidIndex(stageIndex);
				if (const Optional<RenderItemStage*> pStage = GetRenderItemStage(stageIdentifier))
				{
					pStage->OnEnable(graphicsCommandEncoder, perFrameStagingBuffer);
				}
			}
		}
		m_newlyEnabledRenderItemStagesMask.ClearAll();

		for (const typename RenderItemStageMask::BitIndexType stageIndex : m_newlyVisibleRenderStageItemsMask.GetSetBitsIterator())
		{
			const SceneRenderStageIdentifier stageIdentifier = SceneRenderStageIdentifier::MakeFromValidIndex(stageIndex);
			if (const Optional<RenderItemStage*> pStage = GetRenderItemStage(stageIdentifier))
			{
				Entity::RenderItemMask filteredNewlyVisibleRenderStageItems = m_newlyVisibleRenderStageItems[stageIdentifier];
				if (filteredNewlyVisibleRenderStageItems.AreAnySet())
				{
					for (const uint32 renderItemIndex :
					     m_newlyVisibleRenderStageItems[stageIdentifier].GetSetBitsIterator(0, maximumUsedRenderItemCount))
					{
						const Entity::RenderItemIdentifier renderItemIdentifier = Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex);
						if (m_submittedRenderItemStageMasks[renderItemIdentifier].IsSet(stageIdentifier))
						{
							filteredNewlyVisibleRenderStageItems.Clear(renderItemIdentifier);
						}
						else
						{
							Assert(m_queuedRenderItemStageMasks[renderItemIdentifier].IsSet(stageIdentifier));
						}
					}

					pStage->OnRenderItemsBecomeVisible(filteredNewlyVisibleRenderStageItems, graphicsCommandEncoder, perFrameStagingBuffer);

					for (const uint32 renderItemIndex : filteredNewlyVisibleRenderStageItems.GetSetBitsIterator(0, maximumUsedRenderItemCount))
					{
						const Entity::RenderItemIdentifier renderItemIdentifier = Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex);

						if (!m_submittedRenderItemStageMasks[renderItemIdentifier].IsSet(stageIdentifier))
						{
							m_queuedRenderItemStageMasks[renderItemIdentifier].Clear(stageIdentifier);
						}
					}
				}
				m_newlyVisibleRenderStageItems[stageIdentifier].ClearAll();
				m_newlyVisibleRenderStageItemsMask.Clear(stageIdentifier);
			}
		}

		for (const typename RenderItemStageMask::BitIndexType stageIndex : m_newlyHiddenRenderStageItemsMask.GetSetBitsIterator())
		{
			const SceneRenderStageIdentifier stageIdentifier = SceneRenderStageIdentifier::MakeFromValidIndex(stageIndex);
			if (const Optional<RenderItemStage*> pStage = GetRenderItemStage(stageIdentifier))
			{
				Entity::RenderItemMask filteredNewlyHiddenRenderStageItems = m_newlyHiddenRenderStageItems[stageIdentifier];
				if (filteredNewlyHiddenRenderStageItems.AreAnySet())
				{
					for (const uint32 renderItemIndex :
					     m_newlyHiddenRenderStageItems[stageIdentifier].GetSetBitsIterator(0, maximumUsedRenderItemCount))
					{
						const Entity::RenderItemIdentifier renderItemIdentifier = Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex);
						if (!m_submittedRenderItemStageMasks[renderItemIdentifier].IsSet(stageIdentifier))
						{
							filteredNewlyHiddenRenderStageItems.Clear(renderItemIdentifier);
						}
						Assert(!m_queuedRenderItemStageMasks[renderItemIdentifier].IsSet(stageIdentifier));
					}

					pStage->OnRenderItemsBecomeHidden(filteredNewlyHiddenRenderStageItems, scene, graphicsCommandEncoder, perFrameStagingBuffer);

					for (const uint32 renderItemIndex : filteredNewlyHiddenRenderStageItems.GetSetBitsIterator(0, maximumUsedRenderItemCount))
					{
						const Entity::RenderItemIdentifier renderItemIdentifier = Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex);
						Assert(m_submittedRenderItemStageMasks[renderItemIdentifier].IsSet(stageIdentifier));
						m_submittedRenderItemStageMasks[renderItemIdentifier].Clear(stageIdentifier);
					}
				}
				m_newlyHiddenRenderStageItems[stageIdentifier].ClearAll();
				m_newlyHiddenRenderStageItemsMask.Clear(stageIdentifier);
			}
		}

		for (const typename RenderItemStageMask::BitIndexType stageIndex : m_changedTransformRenderStageItemsMask.GetSetBitsIterator())
		{
			const SceneRenderStageIdentifier stageIdentifier = SceneRenderStageIdentifier::MakeFromValidIndex(stageIndex);
			if (const Optional<RenderItemStage*> pStage = GetRenderItemStage(stageIdentifier))
			{
				Entity::RenderItemMask filteredChangedTransformRenderStageItems = m_changedTransformRenderStageItems[stageIdentifier];
				for (const uint32 renderItemIndex :
				     m_changedTransformRenderStageItems[stageIdentifier].GetSetBitsIterator(0, maximumUsedRenderItemCount))
				{
					const Entity::RenderItemIdentifier renderItemIdentifier = Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex);
					if (!m_submittedRenderItemStageMasks[renderItemIdentifier].IsSet(stageIdentifier))
					{
						filteredChangedTransformRenderStageItems.Clear(renderItemIdentifier);
					}
				}

				if (filteredChangedTransformRenderStageItems.AreAnySet())
				{
					pStage->OnVisibleRenderItemTransformsChanged(
						filteredChangedTransformRenderStageItems,
						graphicsCommandEncoder,
						perFrameStagingBuffer

					);
				}
				m_changedTransformRenderStageItems[stageIdentifier].ClearAll();
				m_changedTransformRenderStageItemsMask.Clear(stageIdentifier);
			}
		}

		for (const typename RenderItemStageMask::BitIndexType stageIndex : m_newlyResetRenderStageItemsMask.GetSetBitsIterator())
		{
			const SceneRenderStageIdentifier stageIdentifier = SceneRenderStageIdentifier::MakeFromValidIndex(stageIndex);
			if (const Optional<RenderItemStage*> pStage = GetRenderItemStage(stageIdentifier))
			{
				Entity::RenderItemMask filteredResetRenderStageItems = m_newlyResetRenderStageItems[stageIdentifier];
				for (const uint32 renderItemIndex : m_newlyResetRenderStageItems[stageIdentifier].GetSetBitsIterator(0, maximumUsedRenderItemCount))
				{
					const Entity::RenderItemIdentifier renderItemIdentifier = Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex);
					if (!m_submittedRenderItemStageMasks[renderItemIdentifier].IsSet(stageIdentifier))
					{
						filteredResetRenderStageItems.Clear(renderItemIdentifier);
					}
				}

				if (filteredResetRenderStageItems.AreAnySet())
				{
					pStage->OnVisibleRenderItemsReset(
						filteredResetRenderStageItems,
						graphicsCommandEncoder,
						perFrameStagingBuffer

					);
				}
				m_newlyResetRenderStageItems[stageIdentifier].ClearAll();
				m_newlyResetRenderStageItemsMask.Clear(stageIdentifier);
			}
		}
	}

	void SceneViewBase::OnTraversalFinished(SceneBase& scene)
	{
		// Transform m_visibleRenderItemsBeforeTraversal to contain no longer visible render items
		m_visibleRenderItemsBeforeTraversal = m_visibleRenderItems;
		m_visibleRenderItemsBeforeTraversal &= ~m_visibleRenderItemsDuringTraversal;

		const typename Entity::RenderItemIdentifier::IndexType maximumUsedRenderItemCount = scene.GetMaximumUsedRenderItemCount();

		for (const uint32 renderItemIndex : m_visibleRenderItemsBeforeTraversal.GetSetBitsIterator(0, maximumUsedRenderItemCount))
		{
			const Entity::RenderItemIdentifier renderItemIdentifier = Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex);
			Assert(m_visibleRenderItemsBeforeTraversal.IsSet(renderItemIdentifier));
			Assert(!m_visibleRenderItemsDuringTraversal.IsSet(renderItemIdentifier));

			if (!renderItemIdentifier.IsValid())
			{
				continue;
			}

			Assert(m_visibleRenderItems.IsSet(renderItemIdentifier));
			m_visibleRenderItems.Clear(renderItemIdentifier);
			m_renderItemComponents[renderItemIdentifier] = Invalid;

			m_newlyVisibleRenderItems.Clear(renderItemIdentifier);
			m_newlyHiddenRenderItems.Set(renderItemIdentifier);

			RenderItemStageMask& queuedStageMask = m_queuedRenderItemStageMasks[renderItemIdentifier];
			for (const typename RenderItemStageMask::BitIndexType stageIndex : queuedStageMask.GetSetBitsIterator())
			{
				const SceneRenderStageIdentifier stageIdentifier = SceneRenderStageIdentifier::MakeFromValidIndex(stageIndex);
				m_newlyVisibleRenderStageItems[stageIdentifier].Clear(renderItemIdentifier);
				m_newlyHiddenRenderStageItems[stageIdentifier].Set(renderItemIdentifier);
			}
			m_newlyHiddenRenderStageItemsMask |= queuedStageMask;

			queuedStageMask.ClearAll();
		}
	}

	PURE_STATICS Optional<RenderItemStage*> SceneViewBase::GetRenderItemStage(const SceneRenderStageIdentifier identifier) const
	{
		return Optional<RenderItemStage*>{
			static_cast<RenderItemStage*>(m_sceneRenderStages[identifier].Get()),
			m_renderItemStagesMask.IsSet(identifier)
		};
	}

	void SceneViewBase::ProcessCameraPropertiesChanged(
		const Rendering::CommandEncoderView graphicsCommandEncoder, PerFrameStagingBuffer& perFrameStagingBuffer

	)
	{
		for (const typename SceneRenderStageIdentifier::IndexType stageIndex : m_cameraPropertyDependentStages.GetSetBitsIterator())
		{
			const SceneRenderStageIdentifier stageIdentifier = SceneRenderStageIdentifier::MakeFromValidIndex(stageIndex);
			m_sceneRenderStages[stageIdentifier]->OnActiveCameraPropertiesChanged(graphicsCommandEncoder, perFrameStagingBuffer);
		}
	}

	void SceneViewBase::ProcessCameraTransformChanged(
		const Rendering::CommandEncoderView graphicsCommandEncoder, PerFrameStagingBuffer& perFrameStagingBuffer

	)
	{
		for (const typename SceneRenderStageIdentifier::IndexType stageIndex : m_cameraPropertyDependentStages.GetSetBitsIterator())
		{
			const SceneRenderStageIdentifier stageIdentifier = SceneRenderStageIdentifier::MakeFromValidIndex(stageIndex);
			if (Optional<SceneRenderStage*> pStage = m_sceneRenderStages[stageIdentifier])
			{
				pStage->OnActiveCameraPropertiesChanged(graphicsCommandEncoder, perFrameStagingBuffer);
			}
		}
	}

	Optional<SceneViewModeBase*> SceneViewBase::FindMode(const Guid typeGuid) const
	{
		for (SceneViewModeBase& mode : m_modes)
		{
			if (mode.GetTypeGuid() == typeGuid)
			{
				return mode;
			}
		}
		return Invalid;
	}

	void SceneViewBase::ChangeMode(const Optional<SceneViewModeBase*> pMode)
	{
		Assert(m_pCurrentMode != pMode);
		if (m_pCurrentMode != nullptr)
		{
			m_previousModeIndex = m_modes.GetIteratorIndex(m_modes.Find(*m_pCurrentMode));
			m_pCurrentMode->OnDeactivated(m_pCurrentScene, *this);
		}

		m_pCurrentMode = pMode;
		if (pMode != nullptr)
		{
			pMode->OnActivated(*this);

			if (m_pCurrentScene != nullptr)
			{
				SceneBase& scene = *m_pCurrentScene;
				pMode->OnSceneAssigned(scene, *this);

				if (m_flags.IsSet(Flags::HasSceneFinishedLoading))
				{
					pMode->OnSceneLoaded(scene, *this);
				}
			}
		}
	}

	void SceneViewBase::OnSceneFinishedLoading(SceneBase& scene)
	{
		m_flags |= Flags::HasSceneFinishedLoading;
		if (m_pCurrentMode != nullptr)
		{
			m_pCurrentMode->OnSceneLoaded(scene, *this);
		}
	}

	void SceneViewBase::ExitCurrentMode()
	{
		if (m_previousModeIndex.IsInvalid())
		{
			return;
		}

		ChangeMode(m_modes[m_previousModeIndex.Get()].Get());
	}
}
