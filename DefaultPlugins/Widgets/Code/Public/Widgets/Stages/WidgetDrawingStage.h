#pragma once

#include <Renderer/Stages/Stage.h>
#include <Renderer/Stages/PerFrameStagingBuffer.h>

#include <Renderer/Wrappers/ImageMapping.h>
#include <Renderer/Wrappers/ImageView.h>

#include <Engine/Tag/TagIdentifier.h>

#include <Widgets/Pipelines/Pipelines.h>
#include <Widgets/Data/Drawable.h>

namespace ngine
{
	struct SceneQuadtreeNode;
}

namespace ngine::Entity
{
	struct Component2D;
}

namespace ngine::Rendering
{
	struct SceneView2D;
	struct ToolWindow;
}

namespace ngine::Widgets
{
	struct Widget;

	struct QuadtreeTraversalStage final : public Rendering::Stage
	{
		inline static constexpr size StagingBufferSize = 1536;

		QuadtreeTraversalStage(Rendering::LogicalDevice& logicalDevice, Rendering::SceneView2D& sceneView);
		virtual ~QuadtreeTraversalStage();

		[[nodiscard]] virtual bool ShouldRecordCommands() const override;
		virtual void RecordCommands(const Rendering::CommandEncoderView commandEncoder) override;
		[[nodiscard]] virtual EnumFlags<Rendering::PipelineStageFlags> GetPipelineStageFlags() const override
		{
			return Rendering::PipelineStageFlags::Transfer;
		}

#if STAGE_DEPENDENCY_PROFILING
		[[nodiscard]] virtual ConstZeroTerminatedStringView GetDebugName() const override
		{
			return "Quadtree Traversal Stage";
		}
#endif
	protected:
		void ProcessTransformedComponent(
			Entity::SceneRegistry& sceneRegistry,
			Entity::Component2D& transformedComponent,
			const Math::Rectanglef area,
			const Rendering::CommandEncoderView graphicsCommandEncoder
		);
		void ProcessHierarchy(
			Entity::SceneRegistry& sceneRegistry,
			SceneQuadtreeNode& node,
			const Math::Rectanglef area,
			const Rendering::CommandEncoderView graphicsCommandEncoder
		);
	protected:
		Rendering::SceneView2D& m_sceneView;
		Rendering::PerFrameStagingBuffer m_perFrameStagingBuffer;
		Tag::Identifier m_renderItemTagIdentifier;
	};

	struct WidgetDrawingStage final : public Rendering::Stage
	{
		inline static constexpr Asset::Guid WidgetRenderTargetGuid = "17f7d04f-fd7b-4e5f-90ef-0dec8d559a93"_asset;
		inline static constexpr Asset::Guid WidgetDepthRenderTargetGuid = "e0e5a7b3-4635-4299-94c3-2b5707aca76f"_asset;

		WidgetDrawingStage(Rendering::LogicalDevice& logicalDevice, Rendering::SceneView2D& sceneView, Rendering::ToolWindow& toolWindow);
		virtual ~WidgetDrawingStage();
	protected:
		virtual void OnBeforeRenderPassDestroyed() override;
		[[nodiscard]] virtual Threading::JobBatch AssignRenderPass(
			const Rendering::RenderPassView, const Math::Rectangleui outputArea, const Math::Rectangleui fullRenderArea, const uint8 subpassIndex
		) override;

		[[nodiscard]] virtual bool ShouldRecordCommands() const override;
		virtual void OnBeforeRecordCommands(const Rendering::CommandEncoderView commandEncoder) override;
		[[nodiscard]] virtual EnumFlags<Rendering::PipelineStageFlags> GetPipelineStageFlags() const override
		{
			return Rendering::PipelineStageFlags::ColorAttachmentOutput | Rendering::PipelineStageFlags::LateFragmentTests;
		}
		virtual void RecordRenderPassCommands(
			Rendering::RenderCommandEncoder&, const Rendering::ViewMatrices&, const Math::Rectangleui renderArea, const uint8 subpassIndex
		) override;
		virtual void OnAfterRecordCommands(const Rendering::CommandEncoderView commandEncoder) override;
		[[nodiscard]] virtual uint32 GetMaximumPushConstantInstanceCount() const override;

#if STAGE_DEPENDENCY_PROFILING
		[[nodiscard]] virtual ConstZeroTerminatedStringView GetDebugName() const override
		{
			return "Draw Widgets to Render Target Stage";
		}
#endif
	protected:
		Rendering::SceneView2D& m_sceneView;

		Rendering::Pipelines m_drawingPipelines;

		enum class DrawableType : uint8
		{
			Circle,
			Grid,
			Line,
			Rectangle,
			RoundedRectangle,
			Image,
			Text
		};

		struct QueuedDraw
		{
			ReferenceWrapper<Widgets::Widget> widget;
			DrawableType drawableType;
			float depth;
			Math::Rectanglei area;
		};
		Vector<QueuedDraw> m_queuedDraws;
	};
}
