#pragma once

#include <Common/Asset/Guid.h>
#include <Common/Threading/AtomicInteger.h>
#include <Common/Threading/AtomicBool.h>

#include <Renderer/Stages/RenderItemStage.h>

#include <Renderer/Wrappers/RenderPass.h>
#include <Renderer/Wrappers/Framebuffer.h>
#include <Renderer/Descriptors/DescriptorSet.h>
#include <Renderer/Wrappers/Sampler.h>
#include <Renderer/Wrappers/ImageMapping.h>
#include <Renderer/Assets/Texture/TextureIdentifier.h>
#include <Renderer/Assets/Texture/LoadedTextureFlags.h>

#include <DeferredShading/Pipelines/PBRLightingPipeline.h>
#include <DeferredShading/LightInfo.h>

namespace ngine::Threading
{
	struct EngineJobRunnerThread;
}

namespace ngine::Entity
{
	struct EnvironmentLightComponent;
}

namespace ngine::Rendering
{
	struct SceneView;
	struct LogicalDevice;
	struct RenderTexture;
	struct ShadowsStage;
	struct TilePopulationStage;
	struct BuildAccelerationStructureStage;
	struct Pass;

	struct PBRLightingStage final : public SceneRenderStage
	{
		inline static constexpr Asset::Guid Guid = "5ECCED71-CCC3-459E-B541-36CE31E69163"_asset;
		inline static constexpr Asset::Guid HDRSceneRenderTargetAssetGuid = "314D236E-1C13-49DA-BB70-1052C613768B"_asset;

		PBRLightingStage(
			SceneView& sceneView,
			const Optional<ShadowsStage*> pShadowsStage,
			TilePopulationStage& tilePopulationStage,
			const Optional<BuildAccelerationStructureStage*> pBuildAccelerationStructureStage
		);
		virtual ~PBRLightingStage();

		using SampledTexturesMaskType = uint16;
		enum class SampledTextures : SampledTexturesMaskType
		{
			TextureBegin,
			BRDF = TextureBegin,
			// DefaultIrradiance,
			// DefaultPrefilteredEnvironment,
			TextureEnd,

			StaticRenderTargetBegin = TextureEnd,
			Clusters = StaticRenderTargetBegin,
			Albedo,
			Normals,
			MaterialProperties,
			Depth,
			ShadowmapArray,
			StaticRenderTargetEnd,

			DynamicRenderTargetBegin = StaticRenderTargetEnd,
			IrradianceArray = DynamicRenderTargetBegin,
			PrefilteredEnvironmentArray,
			DynamicRenderTargetEnd,

			RasterizedCount = DynamicRenderTargetEnd,
			RaytracedCount = RasterizedCount - 1,
			TextureCount = TextureEnd - TextureBegin,
			RasterizedStaticRenderTargetCount = StaticRenderTargetEnd - StaticRenderTargetBegin,
			RaytracedStaticRenderTargetCount = RasterizedStaticRenderTargetCount - 1,
			DynamicRenderTargetCount = DynamicRenderTargetEnd - DynamicRenderTargetBegin,
			RasterizedRenderTargetCount = RasterizedStaticRenderTargetCount + DynamicRenderTargetCount,
			RaytracedRenderTargetCount = RaytracedStaticRenderTargetCount + DynamicRenderTargetCount,

			RaytracedAttachmentsMask = (1 << Clusters) | (1 << Albedo) | (1 << Normals) | (1 << MaterialProperties) | (1 << Depth),
			RasterizedAttachmentsMask = RaytracedAttachmentsMask | (1 << ShadowmapArray),
			StaticAttachmentsMask = (1 << BRDF) | (1 << IrradianceArray) | (1 << PrefilteredEnvironmentArray)
		};
		inline static constexpr uint8 TotalSampledTextureCount = (uint8)SampledTextures::RasterizedCount;

		struct Rasterized
		{
			enum class Attachment : uint8
			{
				SubpassInputAttachmentsBegin,
				Albedo = SubpassInputAttachmentsBegin,
				Normals,
				MaterialProperties,
				Depth,
				SubpassInputAttachmentsEnd,

				ExternalInputAttachmentsBegin = SubpassInputAttachmentsEnd,
				Clusters = ExternalInputAttachmentsBegin,
				ShadowmapArray,
				ExternalInputAttachmentsEnd,

				Count = ExternalInputAttachmentsEnd
			};
		};
		struct Raytraced
		{
			enum class Attachment : uint8
			{
				SubpassInputAttachmentsBegin,
				Albedo = SubpassInputAttachmentsBegin,
				Normals,
				MaterialProperties,
				Depth,
				SubpassInputAttachmentsEnd,

				ExternalInputAttachmentsBegin = SubpassInputAttachmentsEnd,
				Clusters = ExternalInputAttachmentsBegin,
				ExternalInputAttachmentsEnd,

				Count = ExternalInputAttachmentsEnd
			};
		};
		inline static constexpr uint32 EnvironmentResolution = 512;

		[[nodiscard]] Threading::JobBatch LoadFixedResources();

		void OnRenderItemsBecomeVisible(const Entity::RenderItemMask& renderItems);
		void OnVisibleRenderItemsReset(const Entity::RenderItemMask& renderItems);
		void OnRenderItemsBecomeHidden(const Entity::RenderItemMask& renderItems);
		void OnVisibleRenderItemTransformsChanged(const Entity::RenderItemMask& renderItems);
		[[nodiscard]] Threading::JobBatch LoadRenderItemsResources(const Entity::RenderItemMask& renderItems);

		[[nodiscard]] virtual bool ShouldRecordCommands() const override;
	protected:
		// SceneRenderStage
		virtual void OnBeforeRenderPassDestroyed() override;
		[[nodiscard]] virtual Threading::JobBatch AssignRenderPass(
			const RenderPassView,
			[[maybe_unused]] const Math::Rectangleui outputArea,
			[[maybe_unused]] const Math::Rectangleui fullRenderArea,
			[[maybe_unused]] const uint8 subpassIndex
		) override;
		virtual void OnRenderPassAttachmentsLoaded(
			[[maybe_unused]] const Math::Vector2ui resolution,
			[[maybe_unused]] const ArrayView<ArrayView<const ImageMappingView, uint16>, Rendering::FrameIndex> colorAttachmentMappings,
			[[maybe_unused]] const ArrayView<ImageMappingView, Rendering::FrameIndex> depthAttachmentMapping,
			[[maybe_unused]] const ArrayView<ArrayView<const ImageMappingView, uint16>, Rendering::FrameIndex> subpassInputAttachmentMappings,
			[[maybe_unused]] const ArrayView<ArrayView<const ImageMappingView, uint16>, Rendering::FrameIndex> externalInputAttachmentMappings,
			[[maybe_unused]] const ArrayView<const Math::Vector2ui, uint16> externalInputAttachmentResolutions,
			[[maybe_unused]] const ArrayView<ArrayView<const Optional<RenderTexture*>, uint16>, Rendering::FrameIndex> colorAttachments,
			[[maybe_unused]] const uint8 subpassIndex
		) override;

		virtual void OnSceneUnloaded() override;
		virtual void
		OnActiveCameraPropertiesChanged(const CommandEncoderView graphicsCommandEncoder, PerFrameStagingBuffer& perFrameStagingBuffer) override;

		virtual void OnBeforeRecordCommands(const CommandEncoderView) override;
		virtual void RecordRenderPassCommands(
			RenderCommandEncoder&, const ViewMatrices&, const Math::Rectangleui renderArea, const uint8 subpassIndex
		) override;
		[[nodiscard]] virtual EnumFlags<PipelineStageFlags> GetPipelineStageFlags() const override
		{
			return PipelineStageFlags::ColorAttachmentOutput | PipelineStageFlags::EarlyFragmentTests | PipelineStageFlags::LateFragmentTests;
		}
		// ~SceneRenderStage

		void UpdateLightBufferDescriptorSet();
		void PopulateDescriptorSet(const Rendering::DescriptorSetView descriptorSet);
		void LoadEnvironmentLightTextures(const Entity::EnvironmentLightComponent& light, const uint8 arrayLayerIndex);
		[[nodiscard]] Threading::JobBatch LoadEnvironmentLightTextures(
			const TextureIdentifier irradianceTextureIdentifier,
			TextureIdentifier prefilteredEnvironmentTextureIdentifier,
			const uint8 arrayLayerIndex
		);
		void OnTextureLoaded(const uint8 textureIndex, const LogicalDevice& logicalDevice, RenderTexture& texture);
		void OnTexturesLoaded();

#if STAGE_DEPENDENCY_PROFILING
		[[nodiscard]] virtual ConstZeroTerminatedStringView GetDebugName() const override
		{
			return "Deferred Lighting Stage";
		}
#endif
	protected:
		SceneView& m_sceneView;
		Optional<ShadowsStage*> m_pShadowsStage;
		const TilePopulationStage& m_tilePopulationStage;
		Optional<BuildAccelerationStructureStage*> m_pBuildAccelerationStructureStage;

		PBRLightingPipeline m_rasterizedPipeline;
		PBRLightingPipelineRaytraced m_raytracedPipeline;

		Rendering::DescriptorSet m_descriptorSet;

		Array<Sampler, TotalSampledTextureCount> m_samplers;
		Array<ImageMapping, TotalSampledTextureCount> m_ownedImageMappings;
		Array<ImageMappingView, TotalSampledTextureCount> m_imageMappingsViews;
		Array<Entity::RenderItemIdentifier, MaximumEnvironmentLightCount> m_visibleEnvironmentLights;
		Array<Rendering::TextureIdentifier, MaximumEnvironmentLightCount> m_loadedIrradianceTextures;
		Array<Rendering::TextureIdentifier, MaximumEnvironmentLightCount> m_loadedPrefilteredTextures;
		Threading::Atomic<SampledTexturesMaskType> m_loadingTexturesMask{0};
		Threading::Atomic<bool> m_shouldUpdateLightBuffer{true};
		Threading::Atomic<bool> m_loadedAllTextures{false};

		Threading::Mutex m_textureLoadMutex;
		Threading::EngineJobRunnerThread* m_pDescriptorSetLoadingThread = nullptr;
	};
}
