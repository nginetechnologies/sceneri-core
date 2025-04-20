#pragma once

#include <Renderer/Stages/Stage.h>

#include <Renderer/Wrappers/RenderPass.h>
#include <Renderer/Wrappers/Framebuffer.h>
#include <Renderer/Descriptors/DescriptorSet.h>
#include <Renderer/Wrappers/Sampler.h>
#include <Renderer/Wrappers/ImageMapping.h>
#include <Renderer/Wrappers/ImageView.h>
#include <Renderer/Buffers/StorageBuffer.h>
#include <Renderer/Assets/Texture/TextureIdentifier.h>
#include <Renderer/Assets/Texture/LoadedTextureFlags.h>

#include <DeferredShading/Pipelines/SharpenPipeline.h>
#include <DeferredShading/Pipelines/SuperResolutionPipeline.h>
#include <DeferredShading/Pipelines/TAAResolvePipeline.h>
#include <DeferredShading/Pipelines/CompositePostProcessPipeline.h>
#include <DeferredShading/Pipelines/LensFlarePipeline.h>
#include <DeferredShading/Pipelines/DownsamplePipeline.h>
#include <DeferredShading/Pipelines/BlurPipeline.h>

#include <DeferredShading/FSR/fsr_settings.h>

#include <Common/Asset/Guid.h>
#include <Common/Threading/AtomicInteger.h>
#include <Common/Storage/Identifier.h>
#include <Common/EnumFlagOperators.h>

namespace ngine::Threading
{
	struct EngineJobRunnerThread;
	struct JobBatch;
}

#define ENABLE_FSR 0
#define ENABLE_TAA (!PLATFORM_WEB)

namespace ngine::Rendering
{
	struct SceneView;
	struct LogicalDevice;
	struct RenderTexture;
	struct RenderItemStage;
	struct MipMask;

	struct PostProcessStage final : public Rendering::Stage
	{
		inline static constexpr Asset::Guid Guid = "502A3475-BC64-4267-AB2C-0A412B17562C"_asset;
		inline static constexpr uint8 LensFlareResolutionDivider = 4;

		enum class Stages : uint8
		{
			Downsample,
			LensflareGeneration,
			LensflareHorizontalBlur,
			LensflareVerticalBlur,
			Composite,
			TemporalAAResolve,
			SuperResolution,
			Sharpen,
			Count
		};

		using ImagesMask = uint16;
		enum class Images : ImagesMask
		{
			RenderOutput,

			DynamicRenderTargetBegin,
			Downsampled = DynamicRenderTargetBegin,
			LensFlare,
			Composite,
			TemporalAAHistory,
			SuperResolution,

			HDR,
			TemporalAAVelocity,
			DynamicRenderTargetEnd,
			DynamicRenderTargetCount = DynamicRenderTargetEnd - DynamicRenderTargetBegin,

			DownsampleStageMask = (1 << HDR) | (1 << Downsampled),
			LensFlareGenerationStageMask = (1 << LensFlare),
			CompositeStageMask = ((1 << Composite) * ENABLE_TAA) | ((1 << RenderOutput) * (!ENABLE_TAA)),
			TemporalAAResolveStageMask = (1 << TemporalAAVelocity) | (1 << TemporalAAHistory) | (1 << HDR) | ((1 << RenderOutput) * !ENABLE_FSR),
			SuperResolutionStageMask = (1 << SuperResolution),
			SharpenStageMask = (1 << RenderOutput),

			FixedTextureBegin = DynamicRenderTargetEnd,
			LensDirt = FixedTextureBegin,
			FixedTextureEnd,
			FixedTextureCount = FixedTextureEnd - FixedTextureBegin,

			RequiredFramegraphMask = DownsampleStageMask | LensFlareGenerationStageMask | CompositeStageMask |
			                         ((SuperResolutionStageMask | SharpenStageMask) * ENABLE_FSR) | ((TemporalAAResolveStageMask)*ENABLE_TAA),
			RequiredMask = (1 << LensDirt) | RequiredFramegraphMask,

			Count = FixedTextureEnd
		};

		inline static constexpr Asset::Guid DownsampleRenderTargetAssetGuid = "37DBDCBD-7EF0-4373-9F6B-19CF4F5FD744"_asset;
		inline static constexpr Asset::Guid LensFlareRenderTargetAssetGuid = "1F3D33BF-5DF2-4549-8618-4CFBB3F89CC1"_asset;
		inline static constexpr Asset::Guid CompositeSceneRenderTargetAssetGuid = "a816fa9e-e3fd-49d0-8bb0-469cce3e8a28"_asset;
		inline static constexpr Asset::Guid TAAVelocityRenderTargetAssetGuid = "73666631-a180-4835-b65c-c05b7e6dd9d7"_asset;
		inline static constexpr Asset::Guid TAAHistorySceneRenderTargetAssetGuid = "540a5fc1-4718-48a3-9c9d-bbddf4612483"_asset;
		inline static constexpr Asset::Guid SuperResolutionSceneRenderTargetAssetGuid = "7e23dcee-ca42-49a9-bd30-190fd5d0a0a2"_asset;
		inline static constexpr Asset::Guid LensDirtTextureAssetGuid = "ffbe89a9-8d62-0f28-26f3-f12b7b2d8d76"_asset;

		PostProcessStage(SceneView& sceneView);
		virtual ~PostProcessStage();

		[[nodiscard]] Threading::JobBatch LoadFixedResources();
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
		virtual void OnAfterRecordCommands(const CommandEncoderView) override;
		[[nodiscard]] virtual EnumFlags<PipelineStageFlags> GetPipelineStageFlags() const override
		{
			return PipelineStageFlags::ComputeShader;
		}

#if STAGE_DEPENDENCY_PROFILING
		[[nodiscard]] virtual ConstZeroTerminatedStringView GetDebugName() const override
		{
			return "Post Process Stage";
		}
#endif

		[[nodiscard]] virtual QueueFamily GetRecordedQueueFamily() const override
		{
			return QueueFamily::Graphics;
		}

		[[nodiscard]] virtual uint32 GetMaximumPushConstantInstanceCount() const override
		{
			return (uint8)Stages::Count;
		}
		// ~Stage

		void OnTextureLoaded(const ImagesMask textureIndex, const LogicalDevice& logicalDevice, RenderTexture& texture);
		void OnFinishedLoadingTextures();
		void PopulateDescriptorSets(const FixedArrayView<Rendering::DescriptorSet, (uint8)Stages::Count, Stages> descriptorSets);
		void PopulateSharpenDescriptorSets(const Rendering::DescriptorSetView descriptorSet, const ImageMappingView swapchain);
		void PopulateTemporalAAResolveDescriptorSets(const Rendering::DescriptorSetView descriptorSet, const ImageMappingView swapchain);
		void PopulateCompositeDescriptorSets(const Rendering::DescriptorSetView descriptorSet, const ImageMappingView swapchain);

		void InitGaussianBlurKernel();
	protected:
		SceneView& m_sceneView;

		DownsamplePipeline m_downsamplePipeline;
		BlurPipeline m_blurPipeline;
		LensFlarePipeline m_lensFlarePipeline;
		CompositePostProcessPipeline m_compositePostProcessPipeline;
		TAAResolvePipeline m_temporalAntiAliasingResolvePipeline;
		SuperResolutionPipeline m_superResolutionPipeline;
		SharpenPipeline m_sharpenPipeline;

		Sampler m_bilinearSampler;

		Sampler m_fsrSampler;

		Sampler m_temporalAntiAliasingCompositeSampler;
		Sampler m_temporalAntiAliasingHistorySampler;

		Array<ImageMappingView, (ImagesMask)Images::Count, Images, ImagesMask> m_imageMappings;
		ImageMapping m_lensDirtImageMapping;

		Optional<RenderTexture*> m_pCompositeTexture;
		Optional<RenderTexture*> m_pTemporalAAHistoryTexture;
		Optional<RenderTexture*> m_pResolveOutputTexture;

		Array<DescriptorSet, (uint8)Stages::Count, Stages> m_descriptorSets;

		Threading::Atomic<ImagesMask> m_loadingTextureMask{0};
		bool m_loadedAllTextures = false;

		StorageBuffer m_gaussianWeightBuffer;

		Threading::Mutex m_descriptorMutex;
		Threading::EngineJobRunnerThread* m_pDescriptorSetLoadingThread = nullptr;

		bool m_isFirstFrame{true};

		inline static constexpr uint8 GaussianBlurKernelSize = 5;
		Array<float, GaussianBlurKernelSize + 1> m_gaussianBlurKernel{Memory::Zeroed};
	};
	ENUM_FLAG_OPERATORS(PostProcessStage::Images);
}
