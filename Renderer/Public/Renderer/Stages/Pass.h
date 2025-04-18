#pragma once

#include <Renderer/Stages/Stage.h>
#include <Renderer/Wrappers/RenderPass.h>
#include <Renderer/Wrappers/Framebuffer.h>
#include <Renderer/Commands/ClearValue.h>
#include <Renderer/FrameImageId.h>

#if PROFILE_BUILD
#include <Common/Memory/Containers/String.h>
#endif

#include <Common/Memory/Containers/InlineVector.h>

namespace ngine::Asset
{
	struct Guid;
}

namespace ngine::Threading
{
	struct JobBatch;
}

namespace ngine::Rendering
{
	struct LogicalDevice;
	struct RenderOutput;
	struct Framegraph;
	struct SceneViewDrawer;

	struct Pass : public Stage
	{
		using ClearValues = InlineVector<ClearValue, 8, uint8>;

		Pass(
			LogicalDevice& logicalDevice,
			RenderOutput& renderOutput,
			Framegraph& framegraph,
			ClearValues&& clearValues,
			const uint8 subpassCount
#if STAGE_DEPENDENCY_PROFILING
			,
			String&& debugName
#endif
		);
		virtual ~Pass();

		void AddStage(Stage& stage, const uint8 subpassIndex);

		[[nodiscard]] RenderPassView GetRenderPass() const
		{
			return m_renderPass;
		}

		[[nodiscard]] Threading::JobBatch Initialize(
			LogicalDevice& logicalDevice,
			const ArrayView<const AttachmentDescription, uint8> attachmentDescriptions,
			const ArrayView<const SubpassDescription, uint8> subpassDescriptions,
			const ArrayView<const SubpassDependency, uint8> subpassDependencies,
			const Math::Rectangleui outputArea,
			const Math::Rectangleui fullRenderArea
		);
		void OnPassAttachmentsLoaded(
			LogicalDevice& logicalDevice,
			const ArrayView<ArrayView<const ImageMappingView, uint16>, FrameIndex> attachmentMappings,
			const Math::Vector2ui framebufferSize
		);
	protected:
		virtual void OnBeforeRenderPassDestroyed() override;
		[[nodiscard]] virtual bool ShouldRecordCommands() const override;
		virtual void OnBeforeRecordCommands(const CommandEncoderView) override;
		virtual void RecordCommands(const CommandEncoderView commandEncoder) override;
		virtual void RecordRenderPassCommands(
			RenderCommandEncoder&, const ViewMatrices&, const Math::Rectangleui renderArea, const uint8 subpassIndex
		) override;
		virtual void OnAfterRecordCommands(const CommandEncoderView) override;
		[[nodiscard]] virtual EnumFlags<PipelineStageFlags> GetPipelineStageFlags() const override;

#if STAGE_DEPENDENCY_PROFILING
		[[nodiscard]] virtual ConstZeroTerminatedStringView GetDebugName() const override
		{
			return m_debugName;
		}
#endif
	protected:
		friend Framegraph;

		RenderOutput& m_renderOutput;
		Framegraph& m_framegraph;
		Optional<SceneViewDrawer*> m_pSceneViewDrawer;

		ClearValues m_clearColors;
#if STAGE_DEPENDENCY_PROFILING
		String m_debugName;
#endif
		RenderPass m_renderPass;
		Math::Rectangleui m_renderArea{Math::Zero, Math::Zero};
		InlineVector<Framebuffer, Rendering::MaximumConcurrentFrameCount> m_framebuffers;

		using SubpassStages = InlineVector<ReferenceWrapper<Stage>, 20>;
		FixedSizeInlineVector<SubpassStages, 2, uint8> m_subpassStages;
	};
}
