#pragma once

#include <Common/Asset/Guid.h>
#include <Common/Threading/AtomicInteger.h>
#include <Common/Storage/Identifier.h>
#include <Common/Memory/Containers/FlatVector.h>
#include <Common/Memory/Containers/ForwardDeclarations/ZeroTerminatedStringView.h>

#include <Renderer/Stages/Stage.h>

#include <Renderer/Descriptors/DescriptorSet.h>
#include <Renderer/Wrappers/Sampler.h>
#include <Renderer/Wrappers/ImageMapping.h>
#include <Renderer/Wrappers/ImageView.h>
#include <Renderer/Assets/Texture/TextureIdentifier.h>
#include <Renderer/Assets/Texture/LoadedTextureFlags.h>

#include <DeferredShading/Pipelines/DepthMinMaxPyramidPipelines.h>

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
	struct RenderItemStage;
	struct MipMask;
	struct ShadowsStage;

	struct DepthMinMaxPyramidStage final : public Rendering::Stage
	{
		inline static constexpr Asset::Guid Guid = "59750AB1-813F-4D09-A8F5-8F53238A684D"_asset;
		inline static constexpr Asset::Guid RenderTargetAssetGuid = "27D18593-B651-4AB1-A2A9-EB29ADECA68F"_asset;

		DepthMinMaxPyramidStage(SceneView& sceneView, const Optional<ShadowsStage*> pShadowsStage);
		virtual ~DepthMinMaxPyramidStage();

		inline static constexpr uint8 SamplerCount = 1;
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
		virtual void RecordComputePassCommands(const ComputeCommandEncoderView, const ViewMatrices&, const uint8 subpassIndex) override;
		[[nodiscard]] virtual EnumFlags<PipelineStageFlags> GetPipelineStageFlags() const override
		{
			return PipelineStageFlags::ComputeShader;
		}

#if STAGE_DEPENDENCY_PROFILING
		[[nodiscard]] virtual ConstZeroTerminatedStringView GetDebugName() const override
		{
			return "Depth Min Max Pyramid Stage";
		}
#endif

		[[nodiscard]] virtual QueueFamily GetRecordedQueueFamily() const override
		{
			return QueueFamily::Graphics;
		}
		// ~Stage
	protected:
		SceneView& m_sceneView;
		Optional<ShadowsStage*> m_pShadowsStage;

		InitialDepthMinMaxPyramidPipeline m_initialPipeline;
		DepthMinMaxPyramidPipeline m_pipeline;

		Threading::EngineJobRunnerThread* m_pDescriptorSetLoadingThread = nullptr;

		FlatVector<DescriptorSet, 33> m_reductionDescriptorSets;
	};
}
