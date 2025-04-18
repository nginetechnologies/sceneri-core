#include "Entity/Scene/SceneRegistry.h"

#include "Engine/Entity/Data/ParentComponent.h"
#include "Engine/Entity/Data/TypeIndex.h"
#include "Engine/Entity/Data/ParentComponent.h"
#include "Engine/Entity/Data/InstanceGuid.h"
#include "Engine/Entity/Data/PreferredIndex.h"
#include "Engine/Entity/Data/Tags.h"

#include "Engine/Entity/Data/RenderItem/StageMask.h"
#include "Engine/Entity/Data/RenderItem/Identifier.h"
#include "Engine/Entity/Data/RenderItem/TransformChangeTracker.h"

#include "Engine/Entity/Data/RenderItem/StaticMeshIdentifier.h"
#include "Engine/Entity/Data/RenderItem/MaterialInstanceIdentifier.h"
#include "Engine/Entity/Data/RenderItem/VisibilityListener.h"

#include "Engine/Entity/Data/WorldTransform.h"
#include "Engine/Entity/Data/LocalTransform3D.h"
#include "Engine/Entity/Data/BoundingBox.h"
#include "Engine/Entity/Data/OctreeNode.h"
#include "Engine/Entity/Data/Flags.h"

#include "Engine/Entity/Data/WorldTransform2D.h"
#include "Engine/Entity/Data/LocalTransform2D.h"
#include "Engine/Entity/Data/QuadtreeNode.h"

#include "Engine/Context/Context.h"
#include "Engine/Context/Reference.h"

#include "Engine/Engine.h"
#include "Engine/Entity/Manager.h"
#include "Engine/Entity/RootSceneComponent.h"
#include "Engine/Entity/ComponentTypeSceneData.h"
#include "Engine/Entity/ComponentTypeInterface.h"

#include <Common/System/Query.h>
#include <Common/Threading/Jobs/JobRunnerThread.h>
#include <Common/Threading/Jobs/JobManager.h>

namespace ngine::Entity
{
	SceneRegistry::SceneRegistry()
		: m_manager(System::Get<Entity::Manager>())
	{
		// Make sure core data components always exist
		m_cachedTypeSceneData[GetSceneDataType<Data::TypeIndex>()] = CreateComponentTypeData<Data::TypeIndex>();
		m_cachedTypeSceneData[GetSceneDataType<Data::Parent>()] = CreateComponentTypeData<Data::Parent>();
		m_cachedTypeSceneData[GetSceneDataType<Data::InstanceGuid>()] = CreateComponentTypeData<Data::InstanceGuid>();
		m_cachedTypeSceneData[GetSceneDataType<Data::Tags>()] = CreateComponentTypeData<Data::Tags>();
		m_cachedTypeSceneData[GetSceneDataType<Data::Flags>()] = CreateComponentTypeData<Data::Flags>();
		m_cachedTypeSceneData[GetSceneDataType<Data::PreferredIndex>()] = CreateComponentTypeData<Data::PreferredIndex>();

		// Render item data
		m_cachedTypeSceneData[GetSceneDataType<Data::RenderItem::StageMask>()] = CreateComponentTypeData<Data::RenderItem::StageMask>();
		m_cachedTypeSceneData[GetSceneDataType<Data::RenderItem::Identifier>()] = CreateComponentTypeData<Data::RenderItem::Identifier>();
		m_cachedTypeSceneData[GetSceneDataType<Data::RenderItem::TransformChangeTracker>()] =
			CreateComponentTypeData<Data::RenderItem::TransformChangeTracker>();

		m_cachedTypeSceneData[GetSceneDataType<Data::RenderItem::StaticMeshIdentifier>()] =
			CreateComponentTypeData<Data::RenderItem::StaticMeshIdentifier>();
		m_cachedTypeSceneData[GetSceneDataType<Data::RenderItem::MaterialInstanceIdentifier>()] =
			CreateComponentTypeData<Data::RenderItem::MaterialInstanceIdentifier>();
		m_cachedTypeSceneData[GetSceneDataType<Data::RenderItem::VisibilityListener>()] =
			CreateComponentTypeData<Data::RenderItem::VisibilityListener>();

		// 3D components
		m_cachedTypeSceneData[GetSceneDataType<Data::WorldTransform>()] = CreateComponentTypeData<Data::WorldTransform>();
		m_cachedTypeSceneData[GetSceneDataType<Data::LocalTransform3D>()] = CreateComponentTypeData<Data::LocalTransform3D>();
		m_cachedTypeSceneData[GetSceneDataType<Data::BoundingBox>()] = CreateComponentTypeData<Data::BoundingBox>();
		m_cachedTypeSceneData[GetSceneDataType<Data::OctreeNode>()] = CreateComponentTypeData<Data::OctreeNode>();

		// 2D components
		m_cachedTypeSceneData[GetSceneDataType<Data::WorldTransform2D>()] = CreateComponentTypeData<Data::WorldTransform2D>();
		m_cachedTypeSceneData[GetSceneDataType<Data::LocalTransform2D>()] = CreateComponentTypeData<Data::LocalTransform2D>();
		m_cachedTypeSceneData[GetSceneDataType<Data::QuadtreeNode>()] = CreateComponentTypeData<Data::QuadtreeNode>();

		// Context components
		m_cachedTypeSceneData[GetSceneDataType<Context::Data::Component>()] = CreateComponentTypeData<Context::Data::Component>();
		m_cachedTypeSceneData[GetSceneDataType<Context::Data::Reference>()] = CreateComponentTypeData<Context::Data::Reference>();

		m_dynamicUpdatesStartStage.AddSubsequentStage(m_dynamicLateUpdatesFinishedStage);
		m_physicsSimulationStartStage.AddSubsequentStage(m_physicsSimulationFinishedStage);

		m_physicsStepStartStage.AddSubsequentStage(m_physicsStepFinishedStage);
	}

	SceneRegistry::~SceneRegistry()
	{
		m_flags |= Flags::CanModifyFrameGraph;
		DestroyComponentTypes();
	}

	void SceneRegistry::Enable(Threading::StageBase& startFrameStage, Threading::StageBase& endFrameStage)
	{
		startFrameStage.AddSubsequentStage(m_physicsSimulationStartStage);
		startFrameStage.AddSubsequentStage(m_dynamicUpdatesStartStage);
		startFrameStage.AddSubsequentStage(m_dynamicRenderUpdatesFinishedStage);
		m_dynamicRenderUpdatesFinishedStage.AddSubsequentStage(endFrameStage);
		m_dynamicLateUpdatesFinishedStage.AddSubsequentStage(endFrameStage);
		m_physicsSimulationFinishedStage.AddSubsequentStage(endFrameStage);
	}

	void SceneRegistry::Disable(Threading::StageBase& startFrameStage, Threading::StageBase& endFrameStage)
	{
		Threading::JobRunnerThread& thread = *Threading::JobRunnerThread::GetCurrent();
		startFrameStage.RemoveSubsequentStage(m_physicsSimulationStartStage, thread, Threading::Job::RemovalFlags{});
		startFrameStage.RemoveSubsequentStage(m_dynamicUpdatesStartStage, thread, Threading::Job::RemovalFlags{});
		startFrameStage.RemoveSubsequentStage(m_dynamicRenderUpdatesFinishedStage, thread, Threading::Job::RemovalFlags{});
		m_dynamicRenderUpdatesFinishedStage.RemoveSubsequentStage(endFrameStage, thread, Threading::Job::RemovalFlags{});
		m_dynamicLateUpdatesFinishedStage.RemoveSubsequentStage(endFrameStage, thread, Threading::Job::RemovalFlags{});
		m_physicsSimulationFinishedStage.RemoveSubsequentStage(endFrameStage, thread, Threading::Job::RemovalFlags{});
	}

	void SceneRegistry::DestroyComponentTypes()
	{
		// Make sure that the octree data is destroyed last
		// Needed as all components can call GetRootSceneComponent in their destructors
		const ComponentTypeIdentifier octreeTypeIdentifier = FindComponentTypeIdentifier(Reflection::GetTypeGuid<RootSceneComponent>());
		Optional<ComponentTypeSceneDataInterface*> pOctreeSceneData;
		pOctreeSceneData = m_componentTypeSceneData[octreeTypeIdentifier].Exchange(pOctreeSceneData);

		for (const Threading::Atomic<ComponentTypeSceneDataInterface*> pComponentTypeSceneData :
		     m_manager.GetRegistry().GetValidElementView(m_componentTypeSceneData.GetView()))
		{
			const Optional<ComponentTypeSceneDataInterface*> pComponentTypeSceneDataInterface = pComponentTypeSceneData.Load();
			if (pComponentTypeSceneDataInterface.IsValid())
			{
				delete pComponentTypeSceneDataInterface.Get();
			}
		}

		if (pOctreeSceneData.IsValid())
		{
			delete pOctreeSceneData.Get();
		}
	}

	void SceneRegistry::DestroyAllComponentInstances()
	{
		for (const Threading::Atomic<ComponentTypeSceneDataInterface*> pComponentTypeSceneData :
		     m_manager.GetRegistry().GetValidElementView(m_componentTypeSceneData.GetView()))
		{
			const Optional<ComponentTypeSceneDataInterface*> pComponentTypeSceneDataInterface = pComponentTypeSceneData.Load();
			if (pComponentTypeSceneDataInterface.IsValid())
			{
				const ComponentTypeInterface& typeInterface = *m_manager.GetRegistry().Get(pComponentTypeSceneDataInterface->GetIdentifier());
				if (typeInterface.GetFlags().IsNotSet(ComponentTypeInterface::TypeFlags::IsDataComponent))
				{
					pComponentTypeSceneDataInterface->DestroyAllInstances();
				}
			}
		}
	}

	//! Queues a function to be called when the frame graph is safe to modify for this scene.
	//! Can execute immediately if it is currently safe!
	void SceneRegistry::ModifyFrameGraph(Function<void(), 24>&& callback)
	{
		if (m_flags.AreAnySet(Flags::CanModifyFrameGraph | Flags::CanModifySceneFrameGraph) && System::Get<Threading::JobManager>().GetJobThreads()[0]->IsExecutingOnThread())
		{
			callback();
		}
		else
		{
			System::Get<Engine>().ModifyFrameGraph(
				[this, callback = Move(callback)]()
				{
					m_flags |= Flags::CanModifyFrameGraph;
					callback();
					m_flags &= ~Flags::CanModifyFrameGraph;
				}
			);
		}
	}

	ComponentTypeIdentifier SceneRegistry::FindComponentTypeIdentifier(const Guid typeGuid) const
	{
		return m_manager.GetRegistry().FindIdentifier(typeGuid);
	}

	Optional<ComponentTypeSceneDataInterface*>
	SceneRegistry::CreateComponentTypeData(const ComponentTypeIdentifier typeIdentifier, ComponentTypeInterface& componentType)
	{
		Assert(m_componentTypeSceneData[typeIdentifier] == nullptr);

		ComponentTypeSceneDataInterface* pComponentTypeSceneData =
			componentType.CreateSceneData(typeIdentifier, m_manager, *this).StealOwnership();
		ComponentTypeSceneDataInterface* pExpected{nullptr};
		const bool wasExchanged = m_componentTypeSceneData[typeIdentifier].CompareExchangeStrong(pExpected, pComponentTypeSceneData);
		Assert(wasExchanged);
		if (UNLIKELY(!wasExchanged))
		{
			delete pComponentTypeSceneData;
		}

		return *pComponentTypeSceneData;
	}

	Optional<ComponentTypeSceneDataInterface*> SceneRegistry::GetOrCreateComponentTypeData(const ComponentTypeIdentifier typeIdentifier)
	{
		return GetOrCreateComponentTypeData(typeIdentifier, *m_manager.GetRegistry().Get(typeIdentifier));
	}

	Optional<ComponentTypeSceneDataInterface*>
	SceneRegistry::GetOrCreateComponentTypeData(const ComponentTypeIdentifier typeIdentifier, ComponentTypeInterface& componentType)
	{
		const Optional<ComponentTypeSceneDataInterface*> pComponentSceneData = m_componentTypeSceneData[typeIdentifier].Load();
		if (pComponentSceneData.IsValid())
		{
			return *pComponentSceneData;
		}

		ComponentTypeSceneDataInterface* pComponentTypeSceneData =
			componentType.CreateSceneData(typeIdentifier, m_manager, *this).StealOwnership();
		ComponentTypeSceneDataInterface* pExpected{nullptr};
		if (LIKELY(m_componentTypeSceneData[typeIdentifier].CompareExchangeStrong(pExpected, pComponentTypeSceneData)))
		{
			return *pComponentTypeSceneData;
		}
		else
		{
			delete pComponentTypeSceneData;
		}
		Assert(pExpected != nullptr);
		return pExpected;
	}

	void ComponentStage::AddSubsequentStage(ComponentStage& otherStage, SceneRegistry& sceneRegistry)
	{
		if (!IsDirectlyFollowedBy(otherStage))
		{
			sceneRegistry.ModifyFrameGraph(
				[this, &otherStage]()
				{
					if (!IsDirectlyFollowedBy(otherStage))
					{
						Job::AddSubsequentStage(otherStage);
					}
				}
			);
		}
	}
}
