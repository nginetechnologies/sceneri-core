#include "Entity/RootSceneComponent.h"
#include "Scene/Scene.h"
#include "Entity/ComponentTypeSceneData.h"
#include "Engine/Entity/ComponentType.h"
#include "Engine/Entity/Data/Tags.h"
#include "Engine/Entity/Data/WorldTransform.h"
#include "Engine/Entity/Data/LocalTransform3D.h"
#include "Engine/Entity/Data/OctreeNode.h"
#include "Engine/Entity/Data/Flags.h"
#include "Engine/Entity/Data/InstanceGuid.h"
#include "Engine/Entity/Component3D.inl"

#include "Engine/Threading/JobRunnerThread.h"

#include <Common/Threading/Jobs/Job.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/Serialization/Guid.h>
#include <Common/Serialization/Deserialize.h>
#include <Common/Math/Ratio.h>
#include <Common/Reflection/Registry.inl>

namespace ngine::Entity
{
	Math::Radius<Math::WorldCoordinateUnitType> ReadRadius(const Serialization::Reader serializer)
	{
		if(const Optional<Serialization::Reader> octreeSerializer =
		       serializer.FindSerializer(Reflection::GetTypeGuid<RootSceneComponent>().ToString().GetView()))
		{
			if (const Optional<Math::Radius<Math::WorldCoordinateUnitType>> radius = octreeSerializer->Read<Math::Radius<Math::WorldCoordinateUnitType>>("radius"))
			{
				return *radius;
			}
		}

		// TODO: Dynamic octree
		return 1024_meters;
	}

	struct DestroyEmptyOctreeNodesJob final : Threading::Job
	{
		DestroyEmptyOctreeNodesJob(RootSceneComponent& rootSceneComponent)
			: Threading::Job(Threading::JobPriority::DestroyEmptyOctreeNodes)
			, m_rootComponent(rootSceneComponent)
		{
		}

#if STAGE_DEPENDENCY_PROFILING
		[[nodiscard]] virtual ConstZeroTerminatedStringView GetDebugName() const override
		{
			return "Destroy Empty Octree Nodes Stage";
		}
#endif
		virtual Result OnExecute([[maybe_unused]] Threading::JobRunnerThread& thread) override
		{
			Threading::UniqueLock lock(m_queuedOctreeNodeRemovalsLock);

			for (decltype(m_queuedOctreeNodeRemovals)::iterator it = m_queuedOctreeNodeRemovals.begin(); it != m_queuedOctreeNodeRemovals.end();
			     ++it)
			{
				SceneOctreeNode& __restrict octreeNode = *it;
				Assert(!octreeNode.HasComponents() & !octreeNode.HasChildren());

				SceneOctreeNode* __restrict pParentNode = octreeNode.GetParent();

				octreeNode.~SceneOctreeNode();

				{
					Threading::UniqueLock poolLock(m_rootComponent.m_poolMutex);
					m_rootComponent.m_pChildNodePool->Deallocate(&octreeNode);
				}

				FlagParentForDeletionIfNecessary(*pParentNode);
			}

			m_queuedOctreeNodeRemovals.Clear();

			return Result::Finished;
		}

		void FlagNodeForDeletionIfNecessary(SceneOctreeNode& __restrict node)
		{
			if (!node.HasComponents() & !node.HasChildren() & !node.IsFlaggedForDeletion() & (&node != &m_rootComponent.m_rootNode))
			{
				if (node.FlagForDeletion())
				{
					Assert(!node.HasComponents() & !node.HasChildren());
					[[maybe_unused]] const SceneOctreeNode::RemovalResult removalResult = node.GetParent()->RemoveChild(node.GetIndex(), node);

					{
						Threading::UniqueLock lock(m_queuedOctreeNodeRemovalsLock);
						Assert(!m_queuedOctreeNodeRemovals.Contains(node));

						// Queue for removal, as we cannot destroy elements that may be currently operated on by a view
						m_queuedOctreeNodeRemovals.EmplaceBack(node);
					}
				}
			}
		}
	protected:
		void FlagParentForDeletionIfNecessary(SceneOctreeNode& __restrict node)
		{
			if (!node.HasComponents() & !node.HasChildren() & !node.IsFlaggedForDeletion() & (&node != &m_rootComponent.m_rootNode))
			{
				if (node.FlagForDeletion())
				{
					Assert(!node.HasComponents() & !node.HasChildren());
					Assert(!m_queuedOctreeNodeRemovals.Contains(node));

					// Queue for removal, as we cannot destroy elements that may be currently operated on by a view
					m_queuedOctreeNodeRemovals.EmplaceBack(node);

					[[maybe_unused]] const SceneOctreeNode::RemovalResult removalResult = node.GetParent()->RemoveChild(node.GetIndex(), node);
					if (removalResult == SceneOctreeNode::RemovalResult::RemovedLastElement)
					{
						FlagParentForDeletionIfNecessary(*node.GetParent());
					}
				}
			}
		}
	protected:
		RootSceneComponent& m_rootComponent;

		Threading::Mutex m_queuedOctreeNodeRemovalsLock;
		inline static constexpr uint16 MaximumQueuedOctreeNodeRemovals = 32768;
		FlatVector<ReferenceWrapper<SceneOctreeNode>, MaximumQueuedOctreeNodeRemovals> m_queuedOctreeNodeRemovals;
	};

	RootSceneComponent::RootSceneComponent(Initializer&& initializer)
		: SceneComponent(
				initializer.m_scene,
				initializer.m_sceneRegistry,
				initializer.m_pParent,
				*this,
				ComponentFlags::IsRootScene | ComponentFlags::SaveToDisk | (ComponentFlags::IsDetachedFromTree * initializer.m_scene.IsTemplate()) |
					(ComponentFlags::IsDisabled * initializer.m_scene.IsDisabled()),
				Guid::Generate(),
				initializer.m_radius,
				{}
			)
		, m_scene(initializer.m_scene)
		, m_radius(initializer.m_radius)
		, m_rootNode(nullptr, 255, Math::WorldCoordinate{Math::Zero}, m_radius)
		, m_pChildNodePool(m_scene->IsTemplate() ? UniquePtr<ChildNodePool>() : UniquePtr<ChildNodePool>::Make())
		, m_destroyEmptyOctreeNodesJob(UniqueRef<DestroyEmptyOctreeNodesJob>::Make(*this))
		, m_octreeNodeSceneData(initializer.m_sceneRegistry.GetCachedSceneData<Entity::Data::OctreeNode>())
		, m_tagComponentTypeSceneData(*initializer.m_sceneRegistry.GetOrCreateComponentTypeData<Entity::Data::Tags>())
	{
		if (initializer.m_scene.IsEnabled())
		{
			m_scene->ModifyFrameGraph(
				[this, &sceneRegistry = initializer.m_sceneRegistry]()
				{
					sceneRegistry.GetDynamicLateUpdatesFinishedStage().AddSubsequentStage(*m_destroyEmptyOctreeNodesJob);
					m_destroyEmptyOctreeNodesJob->AddSubsequentStage(m_scene->GetEndFrameStage());
				}
			);
		}
	}

	RootSceneComponent::RootSceneComponent(const Deserializer& deserializer)
		: SceneComponent(
				deserializer.m_scene,
				deserializer.m_scene.GetEntitySceneRegistry(),
				deserializer.m_pParent,
				*this,
				ComponentFlags::IsRootScene | ComponentFlags::SaveToDisk |
					(ComponentFlags::IsDetachedFromTree * deserializer.m_scene.IsTemplate()) |
					(ComponentFlags::IsDisabled * deserializer.m_scene.IsDisabled()),
				deserializer.m_reader.ReadWithDefaultValue<Guid>("instanceGuid", Guid::Generate()),
				Math::BoundingBox(ReadRadius(deserializer.m_reader)),
				{}
			)
		, m_scene(deserializer.m_scene)
		, m_radius(ReadRadius(deserializer.m_reader))
		, m_rootNode(nullptr, 255, Math::WorldCoordinate{Math::Zero}, m_radius)
		, m_pChildNodePool(m_scene->IsTemplate() ? UniquePtr<ChildNodePool>() : UniquePtr<ChildNodePool>::Make())
		, m_destroyEmptyOctreeNodesJob(UniqueRef<DestroyEmptyOctreeNodesJob>::Make(*this))
		, m_octreeNodeSceneData(deserializer.m_scene.GetEntitySceneRegistry().GetCachedSceneData<Entity::Data::OctreeNode>())
		, m_tagComponentTypeSceneData(*m_scene->GetEntitySceneRegistry().GetOrCreateComponentTypeData<Entity::Data::Tags>())
	{
		if (IsEnabled())
		{
			m_scene->ModifyFrameGraph(
				[this]()
				{
					m_scene->GetEntitySceneRegistry().GetDynamicLateUpdatesFinishedStage().AddSubsequentStage(*m_destroyEmptyOctreeNodesJob);
					m_destroyEmptyOctreeNodesJob->AddSubsequentStage(m_scene->GetEndFrameStage());
				}
			);
		}
	}

	RootSceneComponent::RootSceneComponent(
		Scene& scene, const Entity::ComponentTemplateIdentifier sceneTemplateIdentifier, const Optional<ParentType*> pParent
	)
		: SceneComponent(
				scene,
				scene.GetEntitySceneRegistry(),
				pParent,
				*this,
				ComponentFlags::IsRootScene | ComponentFlags::SaveToDisk | (ComponentFlags::IsDetachedFromTree * scene.IsTemplate()) |
					(ComponentFlags::IsDisabled * scene.IsDisabled()),
				Guid::Generate(),
				Math::Radiusf(4096_meters),
				sceneTemplateIdentifier
			)
		, m_scene(scene)
		, m_radius(4096_meters)
		, m_rootNode(nullptr, 255, Math::WorldCoordinate{Math::Zero}, m_radius)
		, m_pChildNodePool(m_scene->IsTemplate() ? UniquePtr<ChildNodePool>() : UniquePtr<ChildNodePool>::Make())
		, m_destroyEmptyOctreeNodesJob(UniqueRef<DestroyEmptyOctreeNodesJob>::Make(*this))
		, m_octreeNodeSceneData(scene.GetEntitySceneRegistry().GetCachedSceneData<Entity::Data::OctreeNode>())
		, m_tagComponentTypeSceneData(*m_scene->GetEntitySceneRegistry().GetOrCreateComponentTypeData<Entity::Data::Tags>())
	{
		if (IsEnabled())
		{
			m_scene->ModifyFrameGraph(
				[this]()
				{
					m_scene->GetEntitySceneRegistry().GetDynamicLateUpdatesFinishedStage().AddSubsequentStage(*m_destroyEmptyOctreeNodesJob);
					m_destroyEmptyOctreeNodesJob->AddSubsequentStage(m_scene->GetEndFrameStage());
				}
			);
		}
	}

	RootSceneComponent::~RootSceneComponent()
	{
	}

	void RootSceneComponent::SetInstanceGuid(const Guid newInstanceGuid)
	{
		SceneRegistry& sceneRegistry = m_scene->GetEntitySceneRegistry();
		const Guid previousInstanceGuid = GetInstanceGuid(sceneRegistry);

		ComponentTypeSceneData<Data::InstanceGuid>& instanceGuidSceneData = sceneRegistry.GetCachedSceneData<Data::InstanceGuid>();
		Data::InstanceGuid& instanceGuid = instanceGuidSceneData.GetComponentImplementationUnchecked(GetIdentifier());
		instanceGuid = newInstanceGuid;

		ComponentTypeSceneData<RootSceneComponent>& rootSceneData = *sceneRegistry.FindComponentTypeData<RootSceneComponent>();
		rootSceneData.OnInstanceGuidChanged(previousInstanceGuid, newInstanceGuid);
	}

	void RootSceneComponent::OnEnable()
	{
		m_scene->ModifyFrameGraph(
			[this]()
			{
				m_scene->GetEntitySceneRegistry().GetDynamicLateUpdatesFinishedStage().AddSubsequentStage(*m_destroyEmptyOctreeNodesJob);
				m_destroyEmptyOctreeNodesJob->AddSubsequentStage(m_scene->GetEndFrameStage());
			}
		);
	}

	void RootSceneComponent::OnDisable()
	{
		m_scene->GetEntitySceneRegistry()
			.GetDynamicLateUpdatesFinishedStage()
			.RemoveSubsequentStage(m_destroyEmptyOctreeNodesJob, Invalid, Threading::StageBase::RemovalFlags{});
		m_destroyEmptyOctreeNodesJob->RemoveSubsequentStage(m_scene->GetEndFrameStage(), Invalid, Threading::StageBase::RemovalFlags{});
	}

	bool RootSceneComponent::Destroy(SceneRegistry& sceneRegistry)
	{
		ComponentTypeSceneData<Data::Flags>& flagsSceneData = sceneRegistry.GetCachedSceneData<Data::Flags>();
		AtomicEnumFlags<Flags>& flags = flagsSceneData.GetComponentImplementationUnchecked(GetIdentifier());

		if (flags.TrySetFlags(ComponentFlags::IsDestroying))
		{
			{
				const EnumFlags<ComponentFlags> previousFlags = flags.FetchOr(ComponentFlags::IsDisabled);
				if (!previousFlags.AreAnySet(ComponentFlags::IsDisabledFromAnySource))
				{
					DisableInternal(sceneRegistry);
				}
			}

			if (const Optional<HierarchyComponentBase*> pParent = HierarchyComponentBase::GetParentSafe(); pParent.IsValid())
			{
				OnBeforeDetachFromParent();
				pParent->RemoveChildAndClearParent(*this, sceneRegistry);
				pParent->OnChildDetached(*this);
				Assert(!pParent->GetChildren().Contains(*this));
			}

			while (HasChildren())
			{
				Component3D& child = GetChild(0);
				Assert(child.GetParentSafe() == this);
				child.Destroy(sceneRegistry);
			}

			GetRootScene().QueueComponentDestruction(*this);
			return true;
		}
		else
		{
			return false;
		}
	}

	Threading::StageBase& RootSceneComponent::GetOctreeCleanupJob()
	{
		return *m_destroyEmptyOctreeNodesJob;
	}

	Optional<SceneOctreeNode*> RootSceneComponent::GetOrMakeIdealChildNode(
		SceneOctreeNode& node, float componentRadiusSquared, const Math::WorldCoordinate itemLocation, const Math::WorldBoundingBox itemBounds
	)
	{
		componentRadiusSquared = Math::Max(componentRadiusSquared, 0.01f);
		SceneOctreeNode* __restrict pNode = &node;

		Assert(!pNode->IsFlaggedForDeletion());
		Assert(!pNode->WasDeleted());

		constexpr Math::Ratiof treeBalancingRatio = Math::Ratiof(1.f / 16.f);
		float nodeRadius = Math::Sqrt(pNode->GetRadiusSquared());
		float balancedNodeRadius = treeBalancingRatio * nodeRadius;
		float balancedNodeRadiusSquared = balancedNodeRadius * balancedNodeRadius;

		while (componentRadiusSquared < balancedNodeRadiusSquared)
		{
			const uint8 childIndex = pNode->GetChildIndexAtCoordinate(itemLocation);

			SceneOctreeNode* pChildNode = nullptr;
			if (pNode->ReserveChild(childIndex, pChildNode))
			{
				{
					Threading::UniqueLock lock(m_poolMutex);
					pChildNode = reinterpret_cast<SceneOctreeNode*>(m_pChildNodePool->Allocate(sizeof(SceneOctreeNode)));
				}

				Assert(pChildNode != nullptr);
				if (LIKELY(pChildNode != nullptr))
				{
					const Math::TVector3<uint8> childRelativeCoordinate = SceneOctreeNode::GetChildRelativeCoordinate(childIndex);
					const Math::Vector3f offset = (Math::Vector3f)childRelativeCoordinate * nodeRadius;
					const Math::WorldCoordinate minimum = pNode->GetCenterCoordinate() - Math::Vector3f(nodeRadius) + offset;
					const Math::WorldCoordinate maximum = minimum + Math::Vector3f(nodeRadius);

					const Math::WorldBoundingBox childNodeBounds = pNode->GetChildBoundingBox(childIndex);
					[[maybe_unused]] const Math::WorldCoordinate childNodeCenterCoordinate = Math::WorldBoundingBox{minimum, maximum}.GetCenter();
					// TODO: Remove
					Assert(childNodeBounds.GetCenter() == childNodeCenterCoordinate);

					const Math::Radiusf childNodeRadius = Math::Radiusf::FromMeters(nodeRadius * 0.5f);

					new (pChildNode) SceneOctreeNode(pNode, childIndex, childNodeBounds.GetCenter(), childNodeRadius);
					pNode->AddChild(childIndex, *pChildNode);
				}
				else
				{
					return Invalid;
				}
			}
			else if (pChildNode == reinterpret_cast<SceneOctreeNode*>(static_cast<uintptr>(0xDEADBEEF)))
			{
				// Child was reserved and is being created, try again
				continue;
			}

			Assert(pChildNode != nullptr);
			pNode = pChildNode;
			Assert(!pNode->IsFlaggedForDeletion());
			Assert(!pNode->WasDeleted());

			pNode->ExpandChildBoundingBox(itemBounds);

			nodeRadius = Math::Sqrt(pNode->GetRadiusSquared());
			balancedNodeRadius = treeBalancingRatio * nodeRadius;
			balancedNodeRadiusSquared = balancedNodeRadius * balancedNodeRadius;
		}

		return pNode;
	}

	void RootSceneComponent::OnComponentWorldLocationOrBoundsChanged(Entity::Component3D& component, Entity::SceneRegistry& sceneRegistry)
	{
		const Entity::ComponentIdentifier componentIdentifier = component.GetIdentifier();
		const Optional<Data::OctreeNode*> pCurrentOctreeNode = m_octreeNodeSceneData.GetComponentImplementation(componentIdentifier);
		if (LIKELY(pCurrentOctreeNode.IsValid()))
		{
			SceneOctreeNode& currentNode = pCurrentOctreeNode->Get();

			const Math::WorldCoordinate newComponentLocation = component.GetWorldLocation(sceneRegistry);
			if (!currentNode.GetStaticBoundingBox().Contains(newComponentLocation) && &currentNode != &m_rootNode)
			{
				SceneOctreeNode* pNewNode = currentNode.GetParent();
				while (pNewNode != &m_rootNode && !pNewNode->GetStaticBoundingBox().Contains(newComponentLocation))
				{
					pNewNode = pNewNode->GetParent();
				}

				const Math::WorldBoundingBox componentBounds = component.GetWorldBoundingBox(sceneRegistry);
				const Math::WorldCoordinateUnitType componentRadiusSquared = componentBounds.GetRadiusSquared();
				const Optional<SceneOctreeNode*> pNewOctreeNode =
					GetOrMakeIdealChildNode(*pNewNode, componentRadiusSquared, newComponentLocation, componentBounds);
				if (&pCurrentOctreeNode->Get() != pNewOctreeNode)
				{
					const SceneOctreeNode::RemovalResult removalResult = currentNode.RemoveComponent(component);
					if (removalResult == SceneOctreeNode::RemovalResult::RemovedLastElement)
					{
						m_destroyEmptyOctreeNodesJob->FlagNodeForDeletionIfNecessary(currentNode);
					}

					Assert(pNewOctreeNode != nullptr);
					if (LIKELY(pNewOctreeNode != nullptr))
					{
						Tag::Mask mask = Tag::Mask();
						if (Optional<Entity::Data::Tags*> pTagComponent = m_tagComponentTypeSceneData.GetComponentImplementation(componentIdentifier))
						{
							mask = pTagComponent->GetMask();
						}

						pNewOctreeNode->AddComponent(component, mask);
						*pCurrentOctreeNode = *pNewOctreeNode;
					}
					else
					{
						const bool wasRemoved = sceneRegistry.OnDataComponentRemoved(componentIdentifier, m_octreeNodeSceneData.GetIdentifier());
						Assert(wasRemoved);
						if (wasRemoved)
						{
							m_octreeNodeSceneData.OnBeforeRemoveInstance(m_octreeNodeSceneData.GetDataComponentUnsafe(componentIdentifier), component);
							m_octreeNodeSceneData.RemoveInstance(m_octreeNodeSceneData.GetDataComponentUnsafe(componentIdentifier), component);
						}
					}
				}
			}
		}
	}

	void RootSceneComponent::AddComponent(Entity::Component3D& component)
	{
		Entity::SceneRegistry& sceneRegistry = m_scene->GetEntitySceneRegistry();
		const Math::WorldBoundingBox renderItemBounds = component.GetWorldBoundingBox(sceneRegistry);
		const Math::WorldCoordinate renderItemLocation = component.GetWorldLocation(sceneRegistry);
		const float renderItemRadiusSquared = renderItemBounds.GetRadiusSquared();
		const Entity::ComponentIdentifier componentIdentifier = component.GetIdentifier();

		Assert(!sceneRegistry.HasDataComponentOfType(componentIdentifier, m_octreeNodeSceneData.GetIdentifier()));

		Tag::Mask mask = Tag::Mask();
		if (Optional<Entity::Data::Tags*> pTagComponent = m_tagComponentTypeSceneData.GetComponentImplementation(componentIdentifier))
		{
			mask = pTagComponent->GetMask();
		}

		Optional<SceneOctreeNode*> pNewOctreeNode =
			GetOrMakeIdealChildNode(m_rootNode, renderItemRadiusSquared, renderItemLocation, renderItemBounds);
		Assert(pNewOctreeNode != nullptr);
		if (LIKELY(pNewOctreeNode != nullptr))
		{
			pNewOctreeNode->AddComponent(component, mask);
			m_octreeNodeSceneData.CreateInstance(componentIdentifier, Invalid, *pNewOctreeNode);
		}
		else
		{
			Assert(!m_scene->GetEntitySceneRegistry().HasDataComponentOfType(componentIdentifier, m_octreeNodeSceneData.GetIdentifier()));
		}
	}

	void RootSceneComponent::RemoveComponent(Entity::Component3D& component)
	{
		const Entity::ComponentIdentifier componentIdentifier = component.GetIdentifier();
		SceneRegistry& sceneRegistry = m_scene->GetEntitySceneRegistry();
		Assert(sceneRegistry.HasDataComponentOfType(componentIdentifier, m_octreeNodeSceneData.GetIdentifier()));

		const Optional<Data::OctreeNode*> pCurrentOctreeNode = m_octreeNodeSceneData.GetComponentImplementation(componentIdentifier);
		if (LIKELY(pCurrentOctreeNode.IsValid()))
		{
			SceneOctreeNode& __restrict currentNode = pCurrentOctreeNode->Get();

			const SceneOctreeNode::RemovalResult removalResult = currentNode.RemoveComponent(component);
			if (removalResult == SceneOctreeNode::RemovalResult::RemovedLastElement)
			{
				m_destroyEmptyOctreeNodesJob->FlagNodeForDeletionIfNecessary(currentNode);
			}

			const bool wasRemoved = sceneRegistry.OnDataComponentRemoved(componentIdentifier, m_octreeNodeSceneData.GetIdentifier());
			if (Ensure(wasRemoved))
			{
				m_octreeNodeSceneData.OnBeforeRemoveInstance(m_octreeNodeSceneData.GetDataComponentUnsafe(componentIdentifier), component);
				m_octreeNodeSceneData.RemoveInstance(m_octreeNodeSceneData.GetDataComponentUnsafe(componentIdentifier), component);
			}
		}
	}

	void RootSceneComponent::SetWorldTransform(const Math::WorldTransform transform)
	{
		Entity::SceneRegistry& sceneRegistry = m_scene->GetEntitySceneRegistry();
		Entity::ComponentTypeSceneData<Entity::Data::WorldTransform>& worldTransformSceneData =
			sceneRegistry.GetCachedSceneData<Entity::Data::WorldTransform>();
		Entity::ComponentTypeSceneData<Entity::Data::LocalTransform3D>& localTransformSceneData =
			sceneRegistry.GetCachedSceneData<Entity::Data::LocalTransform3D>();
		Entity::ComponentTypeSceneData<Entity::Data::Flags>& flagsSceneData = sceneRegistry.GetCachedSceneData<Data::Flags>();
		SetWorldTransformInternal(worldTransformSceneData, localTransformSceneData, flagsSceneData, transform);
	}

	void RootSceneComponent::SetWorldRotation(const Math::WorldQuaternion rotation)
	{
		Entity::SceneRegistry& sceneRegistry = m_scene->GetEntitySceneRegistry();
		Entity::ComponentTypeSceneData<Entity::Data::WorldTransform>& worldTransformSceneData =
			sceneRegistry.GetCachedSceneData<Entity::Data::WorldTransform>();
		Entity::ComponentTypeSceneData<Entity::Data::LocalTransform3D>& localTransformSceneData =
			sceneRegistry.GetCachedSceneData<Entity::Data::LocalTransform3D>();
		Entity::ComponentTypeSceneData<Entity::Data::Flags>& flagsSceneData = sceneRegistry.GetCachedSceneData<Data::Flags>();
		SetWorldRotationInternal(worldTransformSceneData, localTransformSceneData, flagsSceneData, rotation);
	}

	void RootSceneComponent::SetWorldLocation(const Math::WorldCoordinate location)
	{
		Entity::SceneRegistry& sceneRegistry = m_scene->GetEntitySceneRegistry();
		Entity::ComponentTypeSceneData<Entity::Data::WorldTransform>& worldTransformSceneData =
			sceneRegistry.GetCachedSceneData<Entity::Data::WorldTransform>();
		Entity::ComponentTypeSceneData<Entity::Data::LocalTransform3D>& localTransformSceneData =
			sceneRegistry.GetCachedSceneData<Entity::Data::LocalTransform3D>();
		Entity::ComponentTypeSceneData<Entity::Data::Flags>& flagsSceneData = sceneRegistry.GetCachedSceneData<Data::Flags>();
		SetWorldLocationInternal(worldTransformSceneData, localTransformSceneData, flagsSceneData, location);
	}

	void RootSceneComponent::SetWorldLocationAndRotation(const Math::WorldCoordinate location, const Math::WorldQuaternion rotation)
	{
		Entity::SceneRegistry& sceneRegistry = m_scene->GetEntitySceneRegistry();
		Entity::ComponentTypeSceneData<Entity::Data::WorldTransform>& worldTransformSceneData =
			sceneRegistry.GetCachedSceneData<Entity::Data::WorldTransform>();
		Entity::ComponentTypeSceneData<Entity::Data::LocalTransform3D>& localTransformSceneData =
			sceneRegistry.GetCachedSceneData<Entity::Data::LocalTransform3D>();
		Entity::ComponentTypeSceneData<Entity::Data::Flags>& flagsSceneData = sceneRegistry.GetCachedSceneData<Data::Flags>();
		SetWorldLocationAndRotationInternal(worldTransformSceneData, localTransformSceneData, flagsSceneData, location, rotation);
	}

	void RootSceneComponent::SetWorldScale(const Math::WorldScale scale)
	{
		Entity::SceneRegistry& sceneRegistry = m_scene->GetEntitySceneRegistry();
		Entity::ComponentTypeSceneData<Entity::Data::WorldTransform>& worldTransformSceneData =
			sceneRegistry.GetCachedSceneData<Entity::Data::WorldTransform>();
		Entity::ComponentTypeSceneData<Entity::Data::LocalTransform3D>& localTransformSceneData =
			sceneRegistry.GetCachedSceneData<Entity::Data::LocalTransform3D>();
		Entity::ComponentTypeSceneData<Entity::Data::Flags>& flagsSceneData = sceneRegistry.GetCachedSceneData<Data::Flags>();
		SetWorldScaleInternal(worldTransformSceneData, localTransformSceneData, flagsSceneData, scale);
	}

	[[maybe_unused]] const bool wasRootSceneRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<ComponentType<RootSceneComponent>>::Make());
	[[maybe_unused]] const bool wasRootSceneTypeRegistered = Reflection::Registry::RegisterType<RootSceneComponent>();
}
