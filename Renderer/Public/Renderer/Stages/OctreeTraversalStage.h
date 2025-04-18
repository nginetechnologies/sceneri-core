#pragma once

#include <Renderer/Constants.h>
#include <Renderer/Stages/Stage.h>
#include <Renderer/Stages/PerFrameStagingBuffer.h>

#include <Engine/Tag/TagIdentifier.h>
#include <Engine/Entity/ForwardDeclarations/ComponentTypeSceneData.h>

#include <Common/Math/Transform.h>

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

	struct OctreeTraversalStage final : public Stage
	{
		inline static constexpr size StagingBufferSize = 64 * 150000;

		OctreeTraversalStage(SceneView& sceneView);
		virtual ~OctreeTraversalStage();

		[[nodiscard]] bool HasStartedCulling() const;
	protected:
		// Stage
		virtual bool ShouldRecordCommands() const override;
		virtual void RecordCommands(const CommandEncoderView commandEncoder) override;

		virtual void OnFinishedExecution(Threading::JobRunnerThread& thread) override;

#if STAGE_DEPENDENCY_PROFILING
		[[nodiscard]] virtual ConstZeroTerminatedStringView GetDebugName() const override
		{
			return "Octree Traversal Stage";
		}
#endif
		[[nodiscard]] virtual EnumFlags<PipelineStageFlags> GetPipelineStageFlags() const override
		{
			return PipelineStageFlags::Transfer;
		}
		// ~Stage

		void ProcessTransformedComponentInOctree(
			Entity::Component3D& transformedComponent,
			const Rendering::CommandEncoderView graphicsCommandEncoder,
			Entity::SceneRegistry& sceneRegistry
		);
		void ProcessHierarchyInOctree(
			const SceneOctreeNode& node, const Rendering::CommandEncoderView graphicsCommandEncoder, Entity::SceneRegistry& sceneRegistry
		);
	protected:
		SceneView& m_sceneView;
		Tag::Identifier m_renderItemTagIdentifier;
		PerFrameStagingBuffer m_perFrameStagingBuffer;

		uint8 m_lastFrameIndex = 1u;
	};
}
