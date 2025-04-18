#include "Entity/RootSceneComponent2D.h"

#include <Engine/Entity/Scene/SceneRegistry.h>
#include <Engine/Entity/ComponentTypeSceneData.h>
#include <Engine/Entity/Data/Tags.h>
#include <Engine/Entity/Data/QuadtreeNode.h>

#include <Common/Reflection/Registry.inl>
#include <Common/Threading/Jobs/Job.h>
#include <Common/Threading/Jobs/JobRunnerThread.h>

namespace ngine::Entity
{
	struct DestroyEmptyQuadtreeNodesJob final : Threading::Job
	{
		DestroyEmptyQuadtreeNodesJob(RootSceneComponent2D& rootSceneComponent)
			: Threading::Job(Threading::JobPriority::DestroyEmptyOctreeNodes)
			, m_rootComponent(rootSceneComponent)
		{
		}

#if STAGE_DEPENDENCY_PROFILING
		[[nodiscard]] virtual ConstZeroTerminatedStringView GetDebugName() const override
		{
			return "Destroy Empty Quadtree Nodes Stage";
		}
#endif
		virtual Result OnExecute([[maybe_unused]] Threading::JobRunnerThread& thread) override
		{
			Threading::UniqueLock lock(m_queuedNodeRemovalsLock);

			for (decltype(m_queuedNodeRemovals)::iterator it = m_queuedNodeRemovals.begin(); it != m_queuedNodeRemovals.end(); ++it)
			{
				SceneQuadtreeNode& __restrict node = *it;
				Assert(node.IsEmpty());

				SceneQuadtreeNode& __restrict parentNode = static_cast<SceneQuadtreeNode&>(*node.GetParent());

				delete &node;

				FlagParentForDeletionIfNecessary(parentNode);
			}

			return Result::Finished;
		}

		void FlagNodeForDeletionIfNecessary(SceneQuadtreeNode& __restrict node)
		{
			if (node.IsEmpty() & !node.IsFlaggedForDeletion() & (&node != &m_rootComponent.GetQuadTree()))
			{
				if (node.FlagForDeletion())
				{
					Assert(node.IsEmpty());
					[[maybe_unused]] const SceneQuadtreeNode::RemovalResult removalResult = node.GetParent()->RemoveChild(node.GetIndex(), node);

					{
						Threading::UniqueLock lock(m_queuedNodeRemovalsLock);
						Assert(!m_queuedNodeRemovals.Contains(node));

						// Queue for removal, as we cannot destroy elements that may be currently operated on by a view
						m_queuedNodeRemovals.EmplaceBack(node);
					}
				}
			}
		}
	protected:
		void FlagParentForDeletionIfNecessary(SceneQuadtreeNode& __restrict node)
		{
			if (node.IsEmpty() & !node.IsFlaggedForDeletion() & (&node != &m_rootComponent.GetQuadTree()))
			{
				if (node.FlagForDeletion())
				{
					Assert(node.IsEmpty());
					Assert(!m_queuedNodeRemovals.Contains(node));

					// Queue for removal, as we cannot destroy elements that may be currently operated on by a view
					m_queuedNodeRemovals.EmplaceBack(node);

					[[maybe_unused]] const SceneQuadtreeNode::RemovalResult removalResult = node.GetParent()->RemoveChild(node.GetIndex(), node);
					if (removalResult == SceneQuadtreeNode::RemovalResult::RemovedLastElement)
					{
						FlagParentForDeletionIfNecessary(static_cast<SceneQuadtreeNode&>(*node.GetParent()));
					}
				}
			}
		}
	protected:
		RootSceneComponent2D& m_rootComponent;

		Threading::Mutex m_queuedNodeRemovalsLock;
		inline static constexpr uint16 MaximumQueuedNodeRemovals = 512;
		FlatVector<ReferenceWrapper<SceneQuadtreeNode>, MaximumQueuedNodeRemovals> m_queuedNodeRemovals;
	};

	inline static constexpr Math::Radiusf DefaultQuadtreeRadius = 32768_meters;

	RootSceneComponent2D::RootSceneComponent2D(const Optional<HierarchyComponentBase*> pParent, SceneRegistry& sceneRegistry, Scene2D& scene)
		: Component2D(
				RootSceneComponentType::RootSceneComponent,
				sceneRegistry.AcquireNewComponentIdentifier(),
				pParent,
				sceneRegistry,
				Guid::Generate(),
				Flags::IsRootScene
			)
		, m_scene(scene)
		, m_quadTree(DefaultQuadtreeRadius)
		, m_destroyEmptyQuadtreeNodesJob(UniqueRef<DestroyEmptyQuadtreeNodesJob>::Make(*this))
	{
	}

	RootSceneComponent2D::~RootSceneComponent2D() = default;

	void
	RootSceneComponent2D::AddComponent(Entity::Component2D& component, const float componentDepth, const Math::Rectanglef componentBounds)
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		Entity::ComponentTypeSceneData<Entity::Data::QuadtreeNode>& quadtreeNodeSceneData =
			*sceneRegistry.FindComponentTypeData<Entity::Data::QuadtreeNode>();
		Entity::ComponentTypeSceneData<Entity::Data::Tags>& tagsSceneData = *sceneRegistry.FindComponentTypeData<Entity::Data::Tags>();

		const Entity::ComponentIdentifier componentIdentifier = component.GetIdentifier();

		const float itemRadius(Math::Max(componentBounds.GetSize().x, componentBounds.GetSize().y) * 0.5f);
		SceneQuadtreeNode& node = SceneQuadtree::GetOrMakeIdealChildNode(m_quadTree, itemRadius * itemRadius, componentBounds);
		Tag::Mask mask = Tag::Mask();
		if (Optional<Entity::Data::Tags*> pTagComponent = tagsSceneData.GetComponentImplementation(componentIdentifier))
		{
			mask = pTagComponent->GetMask();
		}

		node.EmplaceElement(component, componentDepth, componentBounds, mask);
		[[maybe_unused]] const Optional<Entity::Data::QuadtreeNode*> pQuadtreeNode =
			component.CreateDataComponent<Entity::Data::QuadtreeNode>(quadtreeNodeSceneData, node);
		Assert(pQuadtreeNode != nullptr);
	}

	void RootSceneComponent2D::OnComponentWorldLocationOrBoundsChanged(
		Entity::Component2D& component, const float componentDepth, const Math::Rectanglef componentBounds, Entity::SceneRegistry& sceneRegistry
	)
	{
		Entity::ComponentTypeSceneData<Entity::Data::QuadtreeNode>& quadtreeNodeSceneData =
			*sceneRegistry.FindComponentTypeData<Entity::Data::QuadtreeNode>();
		Entity::ComponentTypeSceneData<Entity::Data::Tags>& tagsSceneData = *sceneRegistry.FindComponentTypeData<Entity::Data::Tags>();

		const Entity::ComponentIdentifier componentIdentifier = component.GetIdentifier();

		const Optional<Entity::Data::QuadtreeNode*> pCurrentQuadtreeNode = quadtreeNodeSceneData.GetComponentImplementation(componentIdentifier
		);
		Assert(pCurrentQuadtreeNode.IsValid());
		if (LIKELY(pCurrentQuadtreeNode.IsValid()))
		{
			SceneQuadtreeNode& currentNode = pCurrentQuadtreeNode->Get();

			const Math::Vector2f newComponentLocation = componentBounds.GetCenterPosition();
			if (!currentNode.Contains(newComponentLocation) && &currentNode != &m_quadTree)
			{
				SceneQuadtreeNode::BaseType* pNewNode = currentNode.GetParent();
				while (pNewNode != &m_quadTree && !pNewNode->Contains(newComponentLocation))
				{
					pNewNode = pNewNode->GetParent();
				}

				const float itemRadius(Math::Max(componentBounds.GetSize().x, componentBounds.GetSize().y) * 0.5f);

				SceneQuadtreeNode& newNode = static_cast<SceneQuadtreeNode&>(*pNewNode);

				SceneQuadtreeNode* pNewQuadtreeNode = &m_quadTree.GetOrMakeIdealChildNode(newNode, itemRadius * itemRadius, componentBounds);
				if (&pCurrentQuadtreeNode->Get() != pNewQuadtreeNode)
				{
					const SceneQuadtreeNode::RemovalResult removalResult = currentNode.RemoveElement(component);
					if (removalResult == SceneQuadtreeNode::RemovalResult::RemovedLastElement)
					{
						m_destroyEmptyQuadtreeNodesJob->FlagNodeForDeletionIfNecessary(currentNode);
					}

					Assert(pNewQuadtreeNode != nullptr);
					if (LIKELY(pNewQuadtreeNode != nullptr))
					{
						// Assert(pNewQuadtreeNode->GetContentArea().Overlaps(contentArea));
						// Assert(pNewQuadtreeNode->GetContentArea().Mask(contentArea).HasSize());
						Assert(!pNewQuadtreeNode->IsFlaggedForDeletion());

						Tag::Mask mask = Tag::Mask();
						if (Optional<Entity::Data::Tags*> pTagComponent = tagsSceneData.GetComponentImplementation(componentIdentifier))
						{
							mask = pTagComponent->GetMask();
						}

						newNode.EmplaceElement(component, componentDepth, componentBounds, mask);
						*pCurrentQuadtreeNode = newNode;
					}
					else
					{
						const bool wasRemoved = sceneRegistry.OnDataComponentRemoved(componentIdentifier, quadtreeNodeSceneData.GetIdentifier());
						if (Ensure(wasRemoved))
						{
							quadtreeNodeSceneData.OnBeforeRemoveInstance(quadtreeNodeSceneData.GetDataComponentUnsafe(componentIdentifier), component);
							quadtreeNodeSceneData.RemoveInstance(quadtreeNodeSceneData.GetDataComponentUnsafe(componentIdentifier), component);
						}
					}
				}
			}
			else
			{
				currentNode.OnElementContentAreaChanged(componentBounds);
			}
		}
	}

	void RootSceneComponent2D::RemoveComponent(Entity::Component2D& component)
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		Entity::ComponentTypeSceneData<Entity::Data::QuadtreeNode>& quadtreeNodeSceneData =
			*sceneRegistry.FindComponentTypeData<Entity::Data::QuadtreeNode>();

		const Entity::ComponentIdentifier componentIdentifier = component.GetIdentifier();
		const Optional<Entity::Data::QuadtreeNode*> pCurrentQuadtreeNode = quadtreeNodeSceneData.GetComponentImplementation(componentIdentifier
		);
		if (LIKELY(pCurrentQuadtreeNode.IsValid()))
		{
			SceneQuadtreeNode& __restrict currentNode = pCurrentQuadtreeNode->Get();

			const SceneQuadtreeNode::RemovalResult removalResult = currentNode.RemoveElement(component);
			if (removalResult == SceneQuadtreeNode::RemovalResult::RemovedLastElement)
			{
				m_destroyEmptyQuadtreeNodesJob->FlagNodeForDeletionIfNecessary(currentNode);
			}

			const bool wasRemoved = sceneRegistry.OnDataComponentRemoved(componentIdentifier, quadtreeNodeSceneData.GetIdentifier());
			if (Ensure(wasRemoved))
			{
				quadtreeNodeSceneData.OnBeforeRemoveInstance(quadtreeNodeSceneData.GetDataComponentUnsafe(componentIdentifier), component);
				quadtreeNodeSceneData.RemoveInstance(quadtreeNodeSceneData.GetDataComponentUnsafe(componentIdentifier), component);
			}
		}
	}

	Threading::StageBase& RootSceneComponent2D::GetQuadtreeCleanupJob()
	{
		return *m_destroyEmptyQuadtreeNodesJob;
	}

	[[maybe_unused]] const bool wasRootComponent2DTypeRegistered = Reflection::Registry::RegisterType<RootSceneComponent2D>();
	[[maybe_unused]] const bool wasRootComponent2DRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<ComponentType<RootSceneComponent2D>>::Make());
}
