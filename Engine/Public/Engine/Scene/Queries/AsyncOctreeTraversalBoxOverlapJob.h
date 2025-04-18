#pragma once

#include <Common/Threading/Jobs/Job.h>

#include <Common/Function/Function.h>
#include <Common/Memory/CallbackResult.h>
#include <Common/Memory/ReferenceWrapper.h>
#include <Common/Math/Primitives/WorldBoundingBox.h>

#include <Engine/Tag/TagMask.h>
#include <Engine/Entity/ForwardDeclarations/ComponentTypeSceneData.h>

namespace ngine
{
	struct SceneOctreeNode;
}

namespace ngine::Entity
{
	struct Component3D;
	struct SceneRegistry;

	namespace Data
	{
		struct Tags;
	}
}

namespace ngine::SceneQueries
{
	struct AsyncOctreeTraversalBoxOverlapJob final : public Threading::Job
	{
		using Callback = Function<Memory::CallbackResult(const Optional<Entity::Component3D*>), 24>;

		AsyncOctreeTraversalBoxOverlapJob(
			Entity::SceneRegistry& sceneRegistry,
			const SceneOctreeNode& octreeNode,
			const Tag::Mask tagMask,
			const Math::WorldBoundingBox searchBounds,
			Callback&& callback,
			const Priority priority
		);

		virtual Result OnExecute(Threading::JobRunnerThread&) override;
	protected:
		[[nodiscard]] Memory::CallbackResult
		ProcessHierarchyInOctree(const SceneOctreeNode& node, Entity::ComponentTypeSceneData<Entity::Data::Tags>& tagsSceneData);

		Memory::CallbackResult
		ProcessComponent(Entity::Component3D& component, Entity::ComponentTypeSceneData<Entity::Data::Tags>& tagsSceneData);
	protected:
		Entity::SceneRegistry& m_sceneRegistry;
		ReferenceWrapper<const SceneOctreeNode> m_initialOctreeNode;
		const Tag::Mask m_tagMask;
		const Math::WorldBoundingBox m_searchBounds;
		const Callback m_callback;
		bool m_foundAny{false};
	};
}
