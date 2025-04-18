#pragma once

#include <Common/Threading/Jobs/Job.h>
#include <Common/Storage/IdentifierArray.h>
#include <Common/Threading/Jobs/JobBatch.h>
#include <Renderer/Stages/RenderItemStageMask.h>
#include <Engine/Entity/RenderItemIdentifier.h>
#include <Engine/Entity/RenderItemMask.h>

namespace ngine
{
	struct SceneOctreeNode;
}

namespace ngine::Entity
{
	struct Component3D;
	struct SceneRegistry;
}

namespace ngine::Rendering
{
	struct SceneView;
	struct SceneData;

	struct CommandEncoderView;

	struct OctreeTraversalLoadStage final : public Threading::Job
	{
		OctreeTraversalLoadStage(SceneView& sceneView);
		virtual ~OctreeTraversalLoadStage();

		[[nodiscard]] const Threading::JobBatch& GetJobBatch() const
		{
			return m_jobBatch;
		};
	protected:
		// Job
		virtual Result OnExecute(Threading::JobRunnerThread&) override;
		// ~Job

		void ProcessTransformedComponentInOctree(Entity::Component3D& transformedComponent, Entity::SceneRegistry& sceneRegistry);
		void ProcessHierarchyInOctree(const SceneOctreeNode& node, Entity::SceneRegistry& sceneRegistry);
	protected:
		SceneView& m_sceneView;
		Threading::JobBatch m_jobBatch;

		TIdentifierArray<Entity::RenderItemMask, SceneRenderStageIdentifier> m_visibleRenderStageItems = Memory::Zeroed;
		RenderItemStageMask m_visibleRenderStageItemsMask;
	};
}
