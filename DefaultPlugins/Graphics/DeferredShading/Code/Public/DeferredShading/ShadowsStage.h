#pragma once

#include <Common/Asset/Guid.h>
#include <Common/Threading/AtomicInteger.h>
#include <Common/Threading/AtomicBool.h>
#include <Common/Math/Matrix4x4.h>
#include <Common/Memory/Containers/FlatVector.h>
#include <Common/Storage/IdentifierMask.h>

#include <Renderer/Stages/RenderItemStage.h>
#include <Renderer/Stages/VisibleStaticMeshes.h>

#include <Renderer/Wrappers/RenderPass.h>
#include <Renderer/Wrappers/Framebuffer.h>
#include <Renderer/Descriptors/DescriptorSet.h>
#include <Renderer/Wrappers/Sampler.h>
#include <Renderer/Wrappers/ImageMapping.h>
#include <Renderer/Wrappers/ImageView.h>

#include <Renderer/Buffers/StorageBuffer.h>
#include <Renderer/Buffers/StagingBuffer.h>
#include <Renderer/Assets/Texture/TextureIdentifier.h>
#include <Renderer/Assets/Texture/LoadedTextureFlags.h>
#include <Renderer/Assets/Texture/MipMask.h>
#include <Renderer/Devices/PhysicalDeviceFeatures.h>

#include <DeferredShading/Pipelines/ShadowsPipeline.h>
#include <DeferredShading/Pipelines/SDSMPipelines.h>
#include <DeferredShading/LightTypes.h>
#include <DeferredShading/VisibleLight.h>
#include <DeferredShading/LightGatheringStage.h>

namespace ngine::Entity
{
	struct LightSourceComponent;
	struct SpotLightComponent;
}

namespace ngine::Threading
{
	struct EngineJobRunnerThread;
}

namespace ngine::Rendering
{
	struct SceneView;
	struct LogicalDevice;
	struct RenderTexture;
	struct BuildAccelerationStructureStage;

	struct ShadowsStage final : public Rendering::RenderItemStage
	{
		inline static constexpr Asset::Guid Guid = "{DFCA0EF8-EDED-4660-8ADB-43C0AA8F4E60}"_asset;
		inline static constexpr Asset::Guid RenderTargetGuid = "{42143DF3-F148-4A9E-A257-0E92B65F7B0F}"_asset;

		enum class State : uint8
		{
			Disabled,
			Rasterized,
			Raytraced
		};

		inline static constexpr uint8 MaximumShadowmapCount{LightGatheringStage::MaximumShadowmapCount};

		ShadowsStage(
			const State state,
			SceneView& sceneView,
			const Optional<BuildAccelerationStructureStage*> pBuildAccelerationStructureStage,
			LightGatheringStage& lightGatheringStage
		);
		virtual ~ShadowsStage();

		inline static constexpr uint8 SamplerCount = 7;

		using ShadowMapIndexType = LightGatheringStage::ShadowMapIndexType;
		using DirectionalLightIndexType = LightGatheringStage::DirectionalLightIndexType;

		inline static constexpr uint32 ShadowMapSize = 1024;

		[[nodiscard]] State GetState() const
		{
			return m_state;
		}

#if ENABLE_SAMPLE_DISTRIBUTION_SHADOW_MAPS
		[[nodiscard]] BufferView GetSDSMShadowSamplingInfoBuffer() const
		{
			return m_sdsmShadowSamplingInfoBuffer;
		}
		[[nodiscard]] size GetSDSMShadowSamplingInfoBufferSize() const
		{
			return m_sdsmShadowSamplingInfoBuffer.GetSize();
		}
#endif
	protected:
		// RenderItemStage
		virtual void OnBeforeRenderPassDestroyed() override;
		virtual void OnGenericPassAttachmentsLoaded(
			[[maybe_unused]] const ArrayView<ArrayView<const ImageMappingView, uint16>, Rendering::FrameIndex> outputAttachmentMappings,
			[[maybe_unused]] const ArrayView<const Math::Vector2ui, uint16> outputAttachmentResolutions,
			[[maybe_unused]] const ArrayView<ArrayView<const ImageMappingView, uint16>, Rendering::FrameIndex> outputInputAttachmentMappings,
			[[maybe_unused]] const ArrayView<const Math::Vector2ui, uint16> outputInputAttachmentResolutions,
			[[maybe_unused]] const ArrayView<ArrayView<const ImageMappingView, uint16>, Rendering::FrameIndex> inputAttachmentMappings,
			[[maybe_unused]] const ArrayView<const Math::Vector2ui, uint16> inputAttachmentResolutions,
			[[maybe_unused]] const ArrayView<ArrayView<const Optional<RenderTexture*>, uint16>, Rendering::FrameIndex> outputAttachments
		) override;

		virtual void OnRenderItemsBecomeVisible(
			const Entity::RenderItemMask& renderItems,
			const Rendering::CommandEncoderView graphicsCommandEncoder,
			PerFrameStagingBuffer& perFrameStagingBuffer
		) override;
		virtual void OnVisibleRenderItemsReset(
			const Entity::RenderItemMask& renderItems,
			const Rendering::CommandEncoderView graphicsCommandEncoder,
			PerFrameStagingBuffer& perFrameStagingBuffer
		) override;
		virtual void OnRenderItemsBecomeHidden(
			const Entity::RenderItemMask& renderItems,
			SceneBase& scene,
			const Rendering::CommandEncoderView graphicsCommandEncoder,
			PerFrameStagingBuffer& perFrameStagingBuffer
		) override;
		virtual void OnVisibleRenderItemTransformsChanged(
			const Entity::RenderItemMask& renderItems,
			const Rendering::CommandEncoderView graphicsCommandEncoder,
			PerFrameStagingBuffer& perFrameStagingBuffer
		) override;
		[[nodiscard]] virtual Threading::JobBatch LoadRenderItemsResources(const Entity::RenderItemMask& renderItems) override;
		virtual void OnSceneUnloaded() override;
		virtual void OnActiveCameraPropertiesChanged(
			const Rendering::CommandEncoderView graphicsCommandEncoder, PerFrameStagingBuffer& perFrameStagingBuffer
		) override;

		[[nodiscard]] virtual bool ShouldRecordCommands() const override;
		virtual void RecordCommands(const CommandEncoderView commandEncoder) override;
		[[nodiscard]] virtual EnumFlags<PipelineStageFlags> GetPipelineStageFlags() const override
		{
			return PipelineStageFlags::GeometryShader | PipelineStageFlags::LateFragmentTests | PipelineStageFlags::ComputeShader;
		}

		virtual void OnDisabled(Rendering::CommandEncoderView, PerFrameStagingBuffer&) override
		{
		}
		virtual void OnEnable(Rendering::CommandEncoderView, PerFrameStagingBuffer&) override
		{
		}
		// ~RenderItemStage

#if STAGE_DEPENDENCY_PROFILING
		[[nodiscard]] virtual ConstZeroTerminatedStringView GetDebugName() const override
		{
			return "Shadows Stage";
		}
#endif

#if ENABLE_SAMPLE_DISTRIBUTION_SHADOW_MAPS
		void AnalyzeDepthBufferToSetupParallelSplitShadowMaps(const CommandEncoderView graphicsCommandEncoder);
#endif

		void UpdateLightBuffer(const Rendering::CommandEncoderView graphicsCommandEncoder, PerFrameStagingBuffer& perFrameStagingBuffer);
	protected:
		State m_state;
		SceneView& m_sceneView;
		Optional<BuildAccelerationStructureStage*> m_pBuildAccelerationStructureStage;
		LightGatheringStage& m_lightGatheringStage;

		RenderPass m_renderPass;

		FlatVector<Framebuffer, MaximumShadowmapCount> m_framebuffers;

		ShadowsPipeline m_pipeline;
#if ENABLE_SAMPLE_DISTRIBUTION_SHADOW_MAPS
		SDSMPhase1Pipeline m_sdsmPhase1Pipeline;
		SDSMPhase2Pipeline m_sdsmPhase2Pipeline;
		SDSMPhase3Pipeline m_sdsmPhase3Pipeline;

		Optional<StagingBuffer> m_sdsmStagingBuffer;
		StorageBuffer m_sdsmLightInfoBuffer;
		StorageBuffer m_sdsmShadowMapsIndicesBuffer;
		StorageBuffer m_sdsmShadowSamplingInfoBuffer;
#endif

		Threading::Mutex m_textureLoadMutex;

		StorageBuffer m_shadowGenInfoBuffer;

		Array<LightGatheringStage::SDSMLightInfo, LightGatheringStage::MaximumDirectionalLightCount> m_sdsmLightInfos;

		Array<Rendering::DescriptorSet, 4> m_descriptorSets;
		Threading::EngineJobRunnerThread* m_pDescriptorSetLoadingThread = nullptr;
		bool m_loadedResources{false};

		Rendering::VisibleStaticMeshes m_visibleStaticMeshes;
	};
}
