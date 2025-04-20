#pragma once

#include <Common/Asset/Guid.h>
#include <Common/Threading/AtomicInteger.h>
#include <Common/Storage/Identifier.h>
#include <Common/Memory/Containers/FlatVector.h>

#include <Renderer/Stages/Stage.h>

#include <Renderer/Wrappers/RenderPass.h>
#include <Renderer/Wrappers/Framebuffer.h>
#include <Renderer/Descriptors/DescriptorSet.h>
#include <Renderer/Wrappers/Sampler.h>
#include <Renderer/Wrappers/ImageMapping.h>
#include <Renderer/Buffers/StorageBuffer.h>
#include <Renderer/Assets/Texture/TextureIdentifier.h>
#include <Renderer/Assets/Texture/LoadedTextureFlags.h>

#include <DeferredShading/Pipelines/SSAOPipeline.h>
#include <DeferredShading/Pipelines/BlurSimplePipeline.h>

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
	struct SceneRenderStage;
	struct MipRange;

	struct SSAOStage final : public Rendering::Stage
	{
		inline static constexpr Asset::Guid Guid = "59750AB1-813F-4D09-A8F5-8F53238A684D"_asset;
		inline static constexpr uint8 SSAOResolutionDivisor = 1;

		enum class Stages : uint8
		{
			SSAO,
			// BlurSimple,
			Count
		};

		enum class Images : uint8
		{
			// SSAO,
			// SSAOBlur,
			HDR,

			Depth,
			Normals,

			Noise,
			Count
		};

		inline static constexpr Asset::Guid RenderTargetAssetGuid = "35A1C957-EFF6-4F67-A661-04E615229EED"_asset;     // SSAO Buffer
		inline static constexpr Asset::Guid RenderTargetBlurAssetGuid = "0be8cfad-d55f-43c9-b9dc-3b569cade1b1"_asset; // SSAO Blur buffer

		inline static constexpr Asset::Guid NoiseTextureAssetGuid = "d07359fb-30c4-36b8-5c77-2182748091f3"_asset;

		SSAOStage(SceneView& sceneView);
		virtual ~SSAOStage();

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
		[[nodiscard]] virtual EnumFlags<PipelineStageFlags> GetPipelineStageFlags() const override
		{
			return PipelineStageFlags::ComputeShader;
		}

#if STAGE_DEPENDENCY_PROFILING
		[[nodiscard]] virtual ConstZeroTerminatedStringView GetDebugName() const override
		{
			return "SSAO Stage";
		}
#endif
		[[nodiscard]] virtual QueueFamily GetRecordedQueueFamily() const override
		{
			return QueueFamily::Graphics;
		}
		// ~Stage

		void OnFinishedLoadingTextures();
		void PopulateDescriptorSets(const FixedArrayView<Rendering::DescriptorSet, (uint8)Stages::Count, Stages> descriptorSets);

		void InitSSAOKernel();
		// void OnAfterResizeOutput();
	protected:
		SceneView& m_sceneView;

		SSAOPipeline m_pipeline;
		// BlurSimplePipeline m_blurSimplePipeline;

		Sampler m_normalsSampler;
		// Sampler m_bilinearSampler; // For blur

		Array<ImageMappingView, (uint8)Images::Count, Images> m_imageMappings;
		ImageMapping m_noiseImageMapping;

		Array<DescriptorSet, (uint8)Stages::Count, Stages> m_descriptorSets;
		Threading::EngineJobRunnerThread* m_pDescriptorSetLoadingThread = nullptr;

		Threading::Atomic<uint8> m_numLoadingTextures = 0;
		bool m_loadedAllTextures = false;

		StorageBuffer m_kernelStorageBuffer;

		Threading::Mutex m_textureLoadMutex;

		struct KernelBuffer
		{
			inline static constexpr size KernelSize = 16;
			Math::Vector3f samples[KernelSize];
		};
		KernelBuffer m_kernelBuffer;
		inline static constexpr size KernelBufferSize = sizeof(KernelBuffer);
	};
}
