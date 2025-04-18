#include "Stages/OctreeTraversalLoadStage.h"

#include <Engine/Scene/Scene.h>
#include <Engine/Entity/CameraComponent.h>
#include <Engine/Entity/RootSceneComponent.h>
#include <Engine/Entity/Data/RenderItem/Identifier.h>
#include <Engine/Entity/Data/RenderItem/StageMask.h>
#include <Engine/Entity/Data/OctreeNode.h>
#include <Engine/Entity/ComponentTypeSceneData.h>
#include <Engine/Scene/SceneOctreeNode.h>

#include <Renderer/Scene/SceneView.h>
#include <Renderer/Devices/LogicalDevice.h>
#include <Renderer/Jobs/QueueSubmissionJob.h>
#include <Renderer/Renderer.h>
#include <Renderer/Stages/RenderItemStage.h>

#include <Common/Threading/Jobs/JobRunnerThread.h>

namespace ngine::Rendering
{
	OctreeTraversalLoadStage::OctreeTraversalLoadStage(SceneView& sceneView)
		: Threading::Job(Threading::JobPriority::OctreeCulling)
		, m_sceneView(sceneView)
		, m_jobBatch(
				*new Threading::IntermediateStage("Octree Load Traversal Start"), *new Threading::IntermediateStage("Octree Load Traversal End")
			)
	{
	}

	OctreeTraversalLoadStage::~OctreeTraversalLoadStage() = default;

	Threading::Job::Result OctreeTraversalLoadStage::OnExecute(Threading::JobRunnerThread&)
	{
		const Entity::CameraComponent& camera = m_sceneView.GetActiveCameraComponent();

		Entity::SceneRegistry& sceneRegistry = m_sceneView.GetScene().GetEntitySceneRegistry();
		Entity::ComponentTypeSceneData<Entity::Data::OctreeNode>& octreeNodeSceneData =
			*sceneRegistry.FindComponentTypeData<Entity::Data::OctreeNode>();
		SceneOctreeNode& cameraOctreeNode = octreeNodeSceneData.GetComponentImplementation(camera.GetIdentifier())->Get();
		Assert(cameraOctreeNode.GetChildBoundingBox().IsVisibleFromFrustum());

		SceneOctreeNode* pNode = &cameraOctreeNode;
		SceneOctreeNode* pNodeChild = nullptr;
		do
		{
			{
				const SceneOctreeNode::ComponentsView components = pNode->GetComponentsView();
				for (Entity::Component3D& transformedComponent : components)
				{
					ProcessTransformedComponentInOctree(transformedComponent, sceneRegistry);
				}
			}

			for (const Optional<SceneOctreeNode*> pChildNode : pNode->GetChildren())
			{
				if (pChildNode != nullptr && pChildNode != pNodeChild)
				{
					if (pChildNode->GetChildBoundingBox().IsVisibleFromFrustum())
					{
						ProcessHierarchyInOctree(*pChildNode, sceneRegistry);
					}
				}
			}

			pNodeChild = pNode;
			pNode = pNode->GetParent();
		} while (pNode != nullptr);

		for (const typename RenderItemStageMask::BitIndexType stageIndex : m_visibleRenderStageItemsMask.GetSetBitsIterator())
		{
			if (const Optional<RenderItemStage*> pStage = m_sceneView.GetRenderItemStage(SceneRenderStageIdentifier::MakeFromValidIndex(stageIndex)))
			{
				Threading::JobBatch resourceLoadBatch =
					pStage->LoadRenderItemsResources(m_visibleRenderStageItems[SceneRenderStageIdentifier::MakeFromValidIndex(stageIndex)]);
				m_jobBatch.QueueAfterStartStage(resourceLoadBatch);
			}
		}

		return Result::Finished;
	}

	void OctreeTraversalLoadStage::ProcessTransformedComponentInOctree(Entity::Component3D& component, Entity::SceneRegistry& sceneRegistry)
	{
		if (component.IsRenderItem(sceneRegistry))
		{
			switch (m_sceneView.ProcessComponentFromOctreeTraversal(component))
			{
				case SceneView::TraversalResult::BecameVisible:
				case SceneView::TraversalResult::RemainedVisible:
				{
					Entity::ComponentTypeSceneData<Entity::Data::RenderItem::Identifier>& renderItemIdentifierSceneData =
						sceneRegistry.GetCachedSceneData<Entity::Data::RenderItem::Identifier>();
					Entity::ComponentTypeSceneData<Entity::Data::RenderItem::StageMask>& renderItemStageMaskSceneData =
						sceneRegistry.GetCachedSceneData<Entity::Data::RenderItem::StageMask>();
					const Entity::RenderItemIdentifier renderItemIdentifier =
						renderItemIdentifierSceneData.GetComponentImplementationUnchecked(component.GetIdentifier());
					const AtomicRenderItemStageMask& stageMask =
						renderItemStageMaskSceneData.GetComponentImplementationUnchecked(component.GetIdentifier());
					m_visibleRenderStageItemsMask |= stageMask;

					for (const typename RenderItemStageMask::BitIndexType stageIndex : stageMask.GetSetBitsIterator())
					{
						m_visibleRenderStageItems[SceneRenderStageIdentifier::MakeFromValidIndex(stageIndex)].Set(renderItemIdentifier);
					}
				}
				break;
				case SceneView::TraversalResult::BecameHidden:
				case SceneView::TraversalResult::RemainedHidden:
				{
					if constexpr (DEBUG_BUILD)
					{
						Entity::ComponentTypeSceneData<Entity::Data::RenderItem::Identifier>& renderItemIdentifierSceneData =
							sceneRegistry.GetCachedSceneData<Entity::Data::RenderItem::Identifier>();
						Entity::ComponentTypeSceneData<Entity::Data::RenderItem::StageMask>& renderItemStageMaskSceneData =
							sceneRegistry.GetCachedSceneData<Entity::Data::RenderItem::StageMask>();
						[[maybe_unused]] const Entity::RenderItemIdentifier renderItemIdentifier =
							renderItemIdentifierSceneData.GetComponentImplementationUnchecked(component.GetIdentifier());
						const AtomicRenderItemStageMask& stageMask =
							renderItemStageMaskSceneData.GetComponentImplementationUnchecked(component.GetIdentifier());
						for ([[maybe_unused]] const typename RenderItemStageMask::BitIndexType stageIndex : stageMask.GetSetBitsIterator())
						{
							Assert(!m_visibleRenderStageItems[SceneRenderStageIdentifier::MakeFromValidIndex(stageIndex)].IsSet(renderItemIdentifier));
						}
					}
				}
				break;
			}
		}
		else if (component.IsRootSceneComponent(sceneRegistry))
		{
			const Entity::RootSceneComponent& rootSceneComponent = component.AsExpected<Entity::RootSceneComponent>(sceneRegistry);

			const SceneOctreeNode& octreeNode = rootSceneComponent.GetRootNode();
			if (rootSceneComponent.GetDynamicOctreeWorldBoundingBox().IsVisibleFromFrustum())
			{
				ProcessHierarchyInOctree(octreeNode, sceneRegistry);
			}
		}
	}

	void OctreeTraversalLoadStage::ProcessHierarchyInOctree(const SceneOctreeNode& node, Entity::SceneRegistry& sceneRegistry)
	{
		for (Entity::Component3D& transformedComponent : node.GetComponentsView())
		{
			ProcessTransformedComponentInOctree(transformedComponent, sceneRegistry);
		}

		for (const Optional<SceneOctreeNode*> pChildNode : node.GetChildren())
		{
			if (pChildNode != nullptr)
			{
				if (pChildNode->GetChildBoundingBox().IsVisibleFromFrustum())
				{
					ProcessHierarchyInOctree(*pChildNode, sceneRegistry);
				}
			}
		}
	}
}
