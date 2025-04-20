#pragma once

#include <Renderer/Stages/Stage.h>
#include <Renderer/Wrappers/RenderPass.h>
#include <Renderer/Wrappers/Framebuffer.h>
#include <Renderer/Pipelines/GraphicsPipeline.h>
#include <Renderer/Descriptors/DescriptorSetLayout.h>
#include <Renderer/Descriptors/DescriptorSet.h>
#include <Renderer/Wrappers/Sampler.h>
#include <Renderer/Wrappers/ImageMapping.h>
#include <Renderer/Format.h>

namespace ngine::Rendering
{
	struct RenderTexture;
	struct RenderTargetCache;
}

namespace ngine::Threading
{
	struct EngineJobRunnerThread;
}

namespace ngine::Widgets
{
	struct CopyToScreenPipeline : public Rendering::DescriptorSetLayout, public Rendering::GraphicsPipeline
	{
		CopyToScreenPipeline(Rendering::LogicalDevice& logicalDevice);

		[[nodiscard]] bool IsValid() const
		{
			return GraphicsPipeline::IsValid() & DescriptorSetLayout::IsValid();
		}

		void Destroy(Rendering::LogicalDevice& logicalDevice);
		[[nodiscard]] Threading::JobBatch CreatePipeline(
			Rendering::LogicalDevice& logicalDevice,
			Rendering::ShaderCache& shaderCache,
			const Rendering::RenderPassView renderPass,
			const Math::Rectangleui outputArea,
			const Math::Rectangleui renderArea,
			const uint8 subpassIndex
		);
	};

	struct CopyToScreenStage final : public Rendering::Stage
	{
		CopyToScreenStage(Rendering::LogicalDevice& logicalDevice, const Guid guid);
		virtual ~CopyToScreenStage();
	protected:
		// Stage
		virtual void OnRenderPassAttachmentsLoaded(
			[[maybe_unused]] const Math::Vector2ui resolution,
			[[maybe_unused]] const ArrayView<ArrayView<const Rendering::ImageMappingView, uint16>, Rendering::FrameIndex> colorAttachmentMappings,
			[[maybe_unused]] const ArrayView<Rendering::ImageMappingView, Rendering::FrameIndex> depthAttachmentMapping,
			[[maybe_unused]] const ArrayView<ArrayView<const Rendering::ImageMappingView, uint16>, Rendering::FrameIndex>
				subpassInputAttachmentMappings,
			[[maybe_unused]] const ArrayView<ArrayView<const Rendering::ImageMappingView, uint16>, Rendering::FrameIndex>
				externalInputAttachmentMappings,
			[[maybe_unused]] const ArrayView<const Math::Vector2ui, uint16> externalInputAttachmentResolutions,
			[[maybe_unused]] const ArrayView<ArrayView<const Optional<Rendering::RenderTexture*>, uint16>, Rendering::FrameIndex>
				colorAttachments,
			[[maybe_unused]] const uint8 subpassIndex
		) override;

		virtual void OnBeforeRenderPassDestroyed() override;
		[[nodiscard]] virtual Threading::JobBatch AssignRenderPass(
			const Rendering::RenderPassView renderPass,
			const Math::Rectangleui outputArea,
			const Math::Rectangleui fullRenderArea,
			const uint8 subpassIndex
		) override;

		virtual void RecordRenderPassCommands(
			Rendering::RenderCommandEncoder&, const Rendering::ViewMatrices&, const Math::Rectangleui renderArea, const uint8 subpassIndex
		) override;
		[[nodiscard]] virtual bool ShouldRecordCommands() const override;

#if STAGE_DEPENDENCY_PROFILING
		[[nodiscard]] virtual ConstZeroTerminatedStringView GetDebugName() const override
		{
			return "Copy to Screen Stage";
		}
#endif

		[[nodiscard]] virtual EnumFlags<Rendering::PipelineStageFlags> GetPipelineStageFlags() const override
		{
			return Rendering::PipelineStageFlags::ColorAttachmentOutput;
		}
		// ~RenderItemStage
	protected:
		Guid m_guid;
		CopyToScreenPipeline m_pipeline;

		Rendering::Sampler m_sampler;

		Rendering::DescriptorSet m_descriptorSet;
		Threading::EngineJobRunnerThread* m_pDescriptorSetLoadingThread = nullptr;
	};
}
