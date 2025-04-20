#pragma once

#include <Common/Asset/Guid.h>
#include <Common/Threading/AtomicInteger.h>
#include <Common/Storage/Identifier.h>
#include <Common/Memory/Containers/FlatVector.h>

#include <Renderer/Stages/Stage.h>

#include <Renderer/Descriptors/DescriptorSet.h>
#include <Renderer/Wrappers/ImageMappingView.h>

#include <DeferredShading/Pipelines/SSRPipeline.h>
#include <DeferredShading/Pipelines/SSRCompositePipeline.h>

namespace ngine::Threading
{
	struct EngineJobRunnerThread;
	struct JobBatch;
}

namespace ngine::Rendering
{
	struct SceneView;
	struct LogicalDevice;
	struct RenderTexture;
	struct MipRange;

	struct SSRStage final : public Rendering::Stage
	{
		inline static constexpr Asset::Guid Guid = "6FC627E2-2ECC-403C-B253-D27409A80044"_asset;
		inline static constexpr Asset::Guid RenderTargetAssetGuid = "F652D9AB-C7E9-4780-95C3-78EF269F1D13"_asset;
		inline static constexpr uint8 SSRResolutionDivisor = 1; // Not yet supported

		enum class Textures : uint8
		{
			GBufferDepth,
			GBufferNormals,
			GBufferMaterialProperties,
			HDRScene,
			Output,
			Count
		};

		SSRStage(SceneView& sceneView);
		virtual ~SSRStage();
	protected:
		// Stage
		virtual void OnBeforeRenderPassDestroyed() override;
		virtual void OnComputePassAttachmentsLoaded(
			[[maybe_unused]] const ArrayView<ArrayView<const ImageMappingView, uint16>, Rendering::FrameIndex> outputAttachmentMappings,
			[[maybe_unused]] const ArrayView<const Math::Vector2ui, uint16> outputAttachmentResolutions,
			[[maybe_unused]] const ArrayView<ArrayView<const Optional<RenderTexture*>, uint16>, Rendering::FrameIndex> outputAttachments,
			[[maybe_unused]] const ArrayView<ArrayView<const ImageMappingView, uint16>, Rendering::FrameIndex> outputInputAttachmentMappings,
			[[maybe_unused]] const ArrayView<const Math::Vector2ui, uint16> outputInputAttachmentResolutions,
			[[maybe_unused]] const ArrayView<ArrayView<const Optional<RenderTexture*>, uint16>, Rendering::FrameIndex> outputInputAttachments,
			[[maybe_unused]] const ArrayView<ArrayView<const ImageMappingView, uint16>, Rendering::FrameIndex> inputAttachmentMappings,
			[[maybe_unused]] const ArrayView<const Math::Vector2ui, uint16> inputAttachmentResolutions,
			[[maybe_unused]] const ArrayView<ArrayView<const Optional<RenderTexture*>, uint16>, Rendering::FrameIndex> inputAttachments,
			[[maybe_unused]] const uint8 subpassIndex
		) override;

		[[nodiscard]] virtual bool ShouldRecordCommands() const override;
		virtual void RecordComputePassCommands(
			const Rendering::ComputeCommandEncoderView, const Rendering::ViewMatrices&, const uint8 subpassIndex
		) override;
		[[nodiscard]] virtual EnumFlags<PipelineStageFlags> GetPipelineStageFlags() const override
		{
			return PipelineStageFlags::ComputeShader;
		}
		// ~Stage

#if STAGE_DEPENDENCY_PROFILING
		[[nodiscard]] virtual ConstZeroTerminatedStringView GetDebugName() const override
		{
			return "SSR Stage";
		}
#endif
		[[nodiscard]] virtual QueueFamily GetRecordedQueueFamily() const override
		{
			return QueueFamily::Graphics;
		}
		void PopulateSSRDescriptorSet(const Rendering::DescriptorSetView descriptorSet);
		void PopulateCompositeSSRDescriptorSet(const Rendering::DescriptorSetView descriptorSet);
	protected:
		SceneView& m_sceneView;
		Array<ImageMappingView, (uint8)Textures::Count, Textures> m_imageMappings;
		Array<ImageMappingView, 2> m_compositeImageMappings;

		Array<DescriptorSet, 2> m_descriptorSets;
		Threading::EngineJobRunnerThread* m_pDescriptorSetLoadingThread = nullptr;

		SSRPipeline m_pipeline;
		SSRCompositePipeline m_compositePipeline;
	};
}
