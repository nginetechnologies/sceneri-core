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
#include <Renderer/Buffers/StorageBuffer.h>
#include <Renderer/Buffers/StagingBuffer.h>
#include <Renderer/Assets/Texture/TextureIdentifier.h>
#include <Renderer/Assets/Texture/LoadedTextureFlags.h>

#include <DeferredShading/Pipelines/TilePopulationPipeline.h>
#include <DeferredShading/LightTypes.h>
#include <DeferredShading/LightInfo.h>
#include <DeferredShading/LightGatheringStage.h>

namespace ngine::Threading
{
	struct EngineJobRunnerThread;
}

namespace ngine::Rendering
{
	struct SceneView;
	struct LogicalDevice;
	struct RenderTexture;
	struct PBRLightingStage;

	struct TilePopulationStage final : public RenderItemStage
	{
		inline static constexpr Asset::Guid Guid = "F141B823-5844-4FBC-B106-0635FF52199C"_asset;
		inline static constexpr Asset::Guid ClustersTextureAssetGuid = "9fcf4ee6-86dd-45dc-90a1-702dabdfe926"_asset;

		TilePopulationStage(SceneView& sceneView);
		virtual ~TilePopulationStage();

		[[nodiscard]] ArrayView<const ReferenceWrapper<const Entity::LightSourceComponent>> GetVisibleLights(const LightTypes lightType) const
		{
			return m_visibleLights[(uint8)lightType];
		}
		[[nodiscard]] FixedArrayView<const StorageBuffer, (uint8)LightTypes::Count - 1> GetLightStorageBuffers() const
		{
			return m_lightStorageBuffers;
		}
		[[nodiscard]] BufferView GetLightStorageBuffer(const LightTypes lightType) const
		{
			return m_lightStorageBuffers[(uint8)lightType];
		}
		[[nodiscard]] size GetLightStorageBufferSize(const LightTypes lightType) const
		{
			return m_lightStorageBufferSizes[(uint8)lightType];
		}

		[[nodiscard]] static Math::Vector2ui CalculateTileSize(const Math::Vector2ui renderResolution);

		void SetPBRLightingStage(PBRLightingStage& stage)
		{
			m_pPBRLightingStage = &stage;
		}

		// RenderItemStage
		[[nodiscard]] virtual bool ShouldRecordCommands() const override;
		// ~RenderItemStage

		[[nodiscard]] const LightGatheringStage& GetLightGatheringStage() const
		{
			return m_lightGatheringStage;
		}
		[[nodiscard]] LightGatheringStage& GetLightGatheringStage()
		{
			return m_lightGatheringStage;
		}
	protected:
		// RenderItemStage
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

		[[nodiscard]] virtual QueueFamily GetRecordedQueueFamily() const override
		{
			return QueueFamily::Compute;
		}

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
			const CommandEncoderView graphicsCommandEncoder,
			PerFrameStagingBuffer& perFrameStagingBuffer
		) override;
		virtual void OnVisibleRenderItemTransformsChanged(
			const Entity::RenderItemMask& renderItems,
			const CommandEncoderView graphicsCommandEncoder,
			PerFrameStagingBuffer& perFrameStagingBuffer
		) override;
		[[nodiscard]] virtual Threading::JobBatch LoadRenderItemsResources(const Entity::RenderItemMask& renderItems) override;
		virtual void OnSceneUnloaded() override;
		virtual void OnActiveCameraPropertiesChanged(
			const Rendering::CommandEncoderView graphicsCommandEncoder, PerFrameStagingBuffer& perFrameStagingBuffer
		) override;

		virtual void OnBeforeRecordCommands(const CommandEncoderView) override;
		virtual void RecordComputePassCommands(
			const Rendering::ComputeCommandEncoderView, const Rendering::ViewMatrices&, const uint8 subpassIndex
		) override;
		[[nodiscard]] virtual EnumFlags<PipelineStageFlags> GetPipelineStageFlags() const override
		{
			return PipelineStageFlags::ComputeShader;
		}

		virtual void OnDisabled(Rendering::CommandEncoderView, PerFrameStagingBuffer&) override
		{
		}
		virtual void OnEnable(Rendering::CommandEncoderView, PerFrameStagingBuffer&) override
		{
		}

		[[nodiscard]] virtual uint32 GetMaximumPushConstantInstanceCount() const override
		{
			return 1;
		}
		// ~RenderItemStage

#if STAGE_DEPENDENCY_PROFILING
		[[nodiscard]] virtual ConstZeroTerminatedStringView GetDebugName() const override
		{
			return "Tile Population Stage";
		}
#endif

		void UpdateLightBuffer(const Rendering::CommandEncoderView graphicsCommandEncoder, PerFrameStagingBuffer&);
		void UpdateLightBufferDescriptorSet();
		void PopulateTileDescriptorSet(const Rendering::DescriptorSetView descriptorSet);
	protected:
		SceneView& m_sceneView;
		LightGatheringStage m_lightGatheringStage;
		Optional<PBRLightingStage*> m_pPBRLightingStage;

		TilePopulationPipeline m_tilePopulationPipeline;

		Rendering::DescriptorSet m_tileDescriptorSet;

		ImageMappingView m_tiledImageMappingView;
		Sampler m_tiledImageSampler;
		Threading::Atomic<bool> m_shouldUpdateLightBuffer{false};

		Array<StorageBuffer, (uint8)LightTypes::Count - 1> m_lightStorageBuffers;
		Array<size, (uint8)LightTypes::Count - 1> m_lightStorageBufferSizes{Memory::Zeroed};
		Array<Vector<ReferenceWrapper<const Entity::LightSourceComponent>>, (uint8)LightTypes::Count> m_visibleLights;
		Threading::Mutex m_descriptorMutex;
		Threading::EngineJobRunnerThread* m_pDescriptorSetLoadingThread = nullptr;

		uint8 m_passIndex = 0;

		Array<LightHeader, (uint8)LightTypes::Count, LightTypes> m_lightHeaders{Memory::Zeroed};

		FlatVector<PointLightInfo, MaximumLightCounts[(uint8)LightTypes::PointLight]> m_pointLights;
		FlatVector<SpotLightInfo, MaximumLightCounts[(uint8)LightTypes::SpotLight]> m_spotLights;
		FlatVector<DirectionalLightInfo, MaximumLightCounts[(uint8)LightTypes::DirectionalLight]> m_directionalLights;
	};
}
