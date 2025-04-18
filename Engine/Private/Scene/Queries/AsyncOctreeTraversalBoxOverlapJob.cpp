#include "Scene/Queries/AsyncOctreeTraversalBoxOverlapJob.h"

#include <Engine/Scene/SceneOctreeNode.h>
#include <Engine/Entity/Component3D.h>
#include <Engine/Entity/Data/Tags.h>
#include <Engine/Entity/Component3D.inl>

#include <Common/Serialization/Reader.h>
#include <Common/Serialization/Writer.h>

namespace ngine::SceneQueries
{
	AsyncOctreeTraversalBoxOverlapJob::AsyncOctreeTraversalBoxOverlapJob(
		Entity::SceneRegistry& sceneRegistry,
		const SceneOctreeNode& octreeNode,
		const Tag::Mask tagMask,
		const Math::WorldBoundingBox searchBounds,
		Callback&& callback,
		const Priority priority
	)
		: Job(priority)
		, m_sceneRegistry(sceneRegistry)
		, m_initialOctreeNode(octreeNode)
		, m_tagMask(tagMask)
		, m_searchBounds(searchBounds)
		, m_callback(Forward<Callback>(callback))
	{
	}

	Threading::Job::Result AsyncOctreeTraversalBoxOverlapJob::OnExecute(Threading::JobRunnerThread&)
	{
		Entity::ComponentTypeSceneData<Entity::Data::Tags>& tagsSceneData = m_sceneRegistry.GetCachedSceneData<Entity::Data::Tags>();

		const SceneOctreeNode* pNode = &(*m_initialOctreeNode);
		do
		{
			if (pNode->ContainsAnyTags(m_tagMask))
			{
				if (ProcessHierarchyInOctree(*pNode, tagsSceneData) == Memory::CallbackResult::Break)
				{
					return Result::FinishedAndDelete;
				}
			}

			pNode = pNode->GetParent();
		} while (pNode != nullptr);

		if (!m_foundAny)
		{
			m_callback(Invalid);
		}
		return Result::FinishedAndDelete;
	}

	Memory::CallbackResult AsyncOctreeTraversalBoxOverlapJob::ProcessHierarchyInOctree(
		const SceneOctreeNode& node, Entity::ComponentTypeSceneData<Entity::Data::Tags>& tagsSceneData
	)
	{
		Assert(node.ContainsAnyTags(m_tagMask));

		{
			const SceneOctreeNode::ComponentsView components = node.GetComponentsView();
			for (Entity::Component3D& transformedComponent : components)
			{
				if (ProcessComponent(transformedComponent, tagsSceneData) == Memory::CallbackResult::Break)
				{
					return Memory::CallbackResult::Break;
				}
			}
		}

		Memory::CallbackResult result = Memory::CallbackResult::Continue;
		for (const Optional<SceneOctreeNode*> pChildNode : node.GetChildren())
		{
			if (pChildNode != nullptr)
			{
				if (pChildNode->ContainsAnyTags(m_tagMask)) // && Math::Overlaps(pChildNode->GetChildBoundingBox(), m_searchBounds))
				{
					if (ProcessHierarchyInOctree(*pChildNode, tagsSceneData) == Memory::CallbackResult::Break)
					{
						result = Memory::CallbackResult::Break;
					}
				}
			}
		}
		return result;
	}

	Memory::CallbackResult AsyncOctreeTraversalBoxOverlapJob::ProcessComponent(
		Entity::Component3D& component, Entity::ComponentTypeSceneData<Entity::Data::Tags>& tagsSceneData
	)
	{
		Tag::Mask mask = Tag::Mask();
		if (Optional<Entity::Data::Tags*> pTagComponent = component.FindDataComponentOfType<Entity::Data::Tags>(tagsSceneData))
		{
			mask = pTagComponent->GetMask();
		}

		if ((mask & m_tagMask).AreNoneSet())
		{
			return Memory::CallbackResult::Continue;
		}
		m_foundAny = true;
		return m_callback(component);
	}
}
