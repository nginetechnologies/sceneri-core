#include "ShadowsStage.h"
#include "BuildAccelerationStructureStage.h"

#include <Common/Threading/Jobs/AsyncJob.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/Math/Power.h>
#include <Common/Memory/AddressOf.h>
#include <Common/Memory/OffsetOf.h>

#include <Engine/Threading/JobRunnerThread.h>

#include <Common/System/Query.h>
#include <Engine/Entity/Lights/SpotLightComponent.h>
#include <Engine/Entity/Lights/DirectionalLightComponent.h>
#include <Engine/Entity/Lights/PointLightComponent.h>
#include <Engine/Entity/Lights/EnvironmentLightComponent.h>
#include <Engine/Entity/CameraComponent.h>
#include <Engine/Scene/Scene.h>

#include <Renderer/Commands/CommandEncoderView.h>
#include <Renderer/Commands/RenderCommandEncoder.h>
#include <Renderer/Commands/BlitCommandEncoder.h>
#include <Renderer/Commands/ComputeCommandEncoder.h>
#include <Renderer/Commands/SingleUseCommandBuffer.h>
#include <Renderer/Commands/ClearValue.h>
#include <Renderer/Buffers/DataToBufferBatch.h>
#include <Renderer/Scene/SceneView.h>
#include <Renderer/Scene/SceneData.h>
#include <Renderer/Scene/SceneViewDrawer.h>
#include <Renderer/Renderer.h>
#include <Renderer/Stages/StartFrameStage.h>
#include <Renderer/Stages/PerFrameStagingBuffer.h>
#include <Renderer/Assets/Texture/RenderTexture.h>
#include <Renderer/RenderOutput/RenderOutput.h>

#include <Renderer/Wrappers/AttachmentReference.h>
#include <Renderer/Wrappers/AttachmentDescription.h>
#include <Renderer/Wrappers/SubpassDependency.h>
#include <Renderer/Wrappers/BufferMemoryBarrier.h>

#include <DeferredShading/FSR/fsr_settings.h>

namespace ngine::Rendering
{
	namespace Shadowmapping
	{
		inline static constexpr Rendering::AttachmentReference DepthAttachment = {
			Rendering::AttachmentReference{0u, Rendering::ImageLayout::DepthStencilAttachmentOptimal}
		};

		inline static Array<Rendering::AttachmentDescription, 1> CreateAttachments()
		{
			return {Rendering::AttachmentDescription{
				Rendering::Format::D16_UNORM,
				SampleCount::One,
				Rendering::AttachmentLoadType::Clear,
				Rendering::AttachmentStoreType::Store,
				Rendering::AttachmentLoadType::Undefined,
				Rendering::AttachmentStoreType::Undefined,
				Rendering::ImageLayout::DepthStencilAttachmentOptimal,
				Rendering::ImageLayout::DepthStencilAttachmentOptimal
			}};
		}

		inline static constexpr Array<Rendering::SubpassDependency, 2> SubpassDependencies = {
			Rendering::SubpassDependency{
				Rendering::ExternalSubpass,
				0,
				Rendering::PipelineStageFlags::TopOfPipe,
				Rendering::PipelineStageFlags::EarlyFragmentTests | Rendering::PipelineStageFlags::LateFragmentTests,
				Rendering::AccessFlags::MemoryRead,
				Rendering::AccessFlags::DepthStencilReadWrite,
				Rendering::DependencyFlags()
			},
			Rendering::SubpassDependency{
				0,
				Rendering::ExternalSubpass,
				Rendering::PipelineStageFlags::EarlyFragmentTests | Rendering::PipelineStageFlags::LateFragmentTests,
				Rendering::PipelineStageFlags::BottomOfPipe,
				Rendering::AccessFlags::DepthStencilReadWrite,
				Rendering::AccessFlags::MemoryRead,
				Rendering::DependencyFlags()
			}
		};
	}

	ShadowsStage::ShadowsStage(
		const State state,
		SceneView& sceneView,
		const Optional<BuildAccelerationStructureStage*> pBuildAccelerationStructureStage,
		LightGatheringStage& lightGatheringStage
	)
		: RenderItemStage(sceneView.GetLogicalDevice(), Threading::JobPriority::Draw)
		, m_state(state)
		, m_sceneView(sceneView)
		, m_pBuildAccelerationStructureStage(pBuildAccelerationStructureStage)
		, m_lightGatheringStage(lightGatheringStage)
		, m_renderPass(
				sceneView.GetLogicalDevice(),
				Shadowmapping::CreateAttachments(),
				{},
				{},
				Shadowmapping::DepthAttachment,
				Shadowmapping::SubpassDependencies
			)
		, m_pipeline(m_sceneView.GetLogicalDevice(), m_sceneView.GetTransformBufferDescriptorSetLayout())
#if ENABLE_SAMPLE_DISTRIBUTION_SHADOW_MAPS
		, m_sdsmPhase1Pipeline(
				m_sceneView.GetLogicalDevice(), m_sceneView.GetLogicalDevice().GetShaderCache(), m_sceneView.GetMatrices().GetDescriptorSetLayout()
			)
		, m_sdsmPhase2Pipeline(
				m_sceneView.GetLogicalDevice(), m_sceneView.GetLogicalDevice().GetShaderCache(), m_sceneView.GetMatrices().GetDescriptorSetLayout()
			)
		, m_sdsmPhase3Pipeline(m_sceneView.GetLogicalDevice(), m_sceneView.GetLogicalDevice().GetShaderCache())
#if !RENDERER_WEBGPU
		, m_sdsmStagingBuffer(StagingBuffer(
				m_sceneView.GetLogicalDevice(),
				m_sceneView.GetLogicalDevice().GetPhysicalDevice(),
				m_sceneView.GetLogicalDevice().GetDeviceMemoryPool(),
				sizeof(LightGatheringStage::SDSMLightInfo) * LightGatheringStage::MaximumDirectionalLightCount,
				StagingBuffer::Flags::TransferSource
			))
#endif
		, m_sdsmLightInfoBuffer(
				m_sceneView.GetLogicalDevice(),
				m_sceneView.GetLogicalDevice().GetPhysicalDevice(),
				m_sceneView.GetLogicalDevice().GetDeviceMemoryPool(),
				sizeof(LightGatheringStage::SDSMLightInfo) * LightGatheringStage::MaximumDirectionalLightCount
			)
		, m_sdsmShadowMapsIndicesBuffer(
				m_sceneView.GetLogicalDevice(),
				m_sceneView.GetLogicalDevice().GetPhysicalDevice(),
				m_sceneView.GetLogicalDevice().GetDeviceMemoryPool(),
				sizeof(uint32) * LightGatheringStage::MaximumDirectionalLightCount * LightGatheringStage::MaximumDirectionalLightCascadeCount
			)
		, m_sdsmShadowSamplingInfoBuffer(
				m_sceneView.GetLogicalDevice(),
				m_sceneView.GetLogicalDevice().GetPhysicalDevice(),
				m_sceneView.GetLogicalDevice().GetDeviceMemoryPool(),
				sizeof(LightGatheringStage::SDSMShadowSamplingInfo) * LightGatheringStage::MaximumDirectionalLightCount
			)
#endif
		, m_shadowGenInfoBuffer(StorageBuffer(
				m_sceneView.GetLogicalDevice(),
				m_sceneView.GetLogicalDevice().GetPhysicalDevice(),
				m_sceneView.GetLogicalDevice().GetDeviceMemoryPool(),
				LightGatheringStage::CalculateLightBufferSize(LightGatheringStage::MaximumShadowmapCount)
			))
	{
		lightGatheringStage.SetShadowsStage(this);

		LogicalDevice& logicalDevice = m_sceneView.GetLogicalDevice();
		const SceneRenderStageIdentifier stageIdentifier = System::Get<Rendering::Renderer>().GetStageCache().FindIdentifier(Guid);
		m_sceneView.RegisterRenderItemStage(stageIdentifier, *this);
		m_sceneView.SetStageDependentOnCameraProperties(stageIdentifier);

		Threading::EngineJobRunnerThread& thread = static_cast<Threading::EngineJobRunnerThread&>(*Threading::JobRunnerThread::GetCurrent());

		const uint32 subpassIndex = 0;
		Threading::JobBatch jobbatch = m_pipeline.CreatePipeline(
			logicalDevice,
			logicalDevice.GetShaderCache(),
			m_renderPass,
			Math::Rectangleui{Math::Zero, {ShadowMapSize, ShadowMapSize}},
			Math::Rectangleui{Math::Zero, {ShadowMapSize, ShadowMapSize}},
			subpassIndex
		);
		thread.Queue(jobbatch);

		const Array<DescriptorSetLayoutView, 4> descriptorSetLayouts =
			{m_pipeline, m_sdsmPhase1Pipeline, m_sdsmPhase2Pipeline, m_sdsmPhase3Pipeline};

		static const LightGatheringStage::Header header{0};

		{
			const Rendering::CommandPoolView commandPool =
				thread.GetRenderData().GetCommandPool(logicalDevice.GetIdentifier(), Rendering::QueueFamily::Transfer);
			Rendering::SingleUseCommandBuffer
				commandBuffer(logicalDevice, commandPool, thread, Rendering::QueueFamily::Transfer, Threading::JobPriority::CreateRenderMesh);

			const CommandEncoderView commandEncoder = commandBuffer;
			const BlitCommandEncoder blitCommandEncoder = commandEncoder.BeginBlit();
			Optional<StagingBuffer> stagingBuffer;
			blitCommandEncoder.RecordCopyDataToBuffer(
				logicalDevice,
				QueueFamily::Graphics,
				Array<const DataToBufferBatch, 1>{
					DataToBufferBatch{m_shadowGenInfoBuffer, Array<const DataToBuffer, 1>{DataToBuffer{0, ConstByteView::Make(header)}}}
				},
				stagingBuffer
			);
			if (stagingBuffer.IsValid())
			{
				commandBuffer.OnFinished = [&logicalDevice, stagingBuffer = Move(*stagingBuffer)]() mutable
				{
					stagingBuffer.Destroy(logicalDevice, logicalDevice.GetDeviceMemoryPool());
				};
			}
		}

		const DescriptorPoolView descriptorPool = thread.GetRenderData().GetDescriptorPool(logicalDevice.GetIdentifier());

		[[maybe_unused]] const bool allocatedDescriptorSets =
			descriptorPool.AllocateDescriptorSets(logicalDevice, descriptorSetLayouts.GetView(), m_descriptorSets);
		m_pDescriptorSetLoadingThread = &thread;
	}

	ShadowsStage::~ShadowsStage()
	{
		LogicalDevice& logicalDevice = m_sceneView.GetLogicalDevice();

		m_renderPass.Destroy(logicalDevice);

		for (Framebuffer& framebuffer : m_framebuffers)
		{
			framebuffer.Destroy(logicalDevice);
		}

		m_pipeline.Destroy(logicalDevice);

#if ENABLE_SAMPLE_DISTRIBUTION_SHADOW_MAPS
		m_sdsmPhase1Pipeline.Destroy(logicalDevice);
		m_sdsmPhase2Pipeline.Destroy(logicalDevice);
		m_sdsmPhase3Pipeline.Destroy(logicalDevice);

		if (m_sdsmStagingBuffer.IsValid())
		{
			Threading::EngineJobRunnerThread& thread = *Threading::EngineJobRunnerThread::GetCurrent();
			thread.GetRenderData().DestroyBuffer(m_logicalDevice.GetIdentifier(), Move(*m_sdsmStagingBuffer));
		}

		m_sdsmLightInfoBuffer.Destroy(logicalDevice, logicalDevice.GetDeviceMemoryPool());
		m_sdsmShadowMapsIndicesBuffer.Destroy(logicalDevice, logicalDevice.GetDeviceMemoryPool());
		m_sdsmShadowSamplingInfoBuffer.Destroy(logicalDevice, logicalDevice.GetDeviceMemoryPool());
#endif

		m_shadowGenInfoBuffer.Destroy(logicalDevice, logicalDevice.GetDeviceMemoryPool());

		Threading::JobRunnerThread* pPreviousDescriptorLoadingThread = m_pDescriptorSetLoadingThread;
		if (pPreviousDescriptorLoadingThread != nullptr)
		{
			Threading::EngineJobRunnerThread& previousLoadingThread =
				static_cast<Threading::EngineJobRunnerThread&>(*pPreviousDescriptorLoadingThread);
			previousLoadingThread.GetRenderData().DestroyDescriptorSets(logicalDevice.GetIdentifier(), m_descriptorSets.GetView());
		}

		m_visibleStaticMeshes.Destroy(logicalDevice);

		Rendering::StageCache& stageCache = System::Get<Rendering::Renderer>().GetStageCache();
		const SceneRenderStageIdentifier stageIdentifier = stageCache.FindIdentifier(Guid);
		m_sceneView.DeregisterRenderItemStage(stageIdentifier);
	}

	void ShadowsStage::OnBeforeRenderPassDestroyed()
	{
		m_loadedResources = false;
		for (Framebuffer& framebuffer : m_framebuffers)
		{
			framebuffer.Destroy(m_sceneView.GetLogicalDevice());
		}
	}

	void ShadowsStage::OnGenericPassAttachmentsLoaded(
		[[maybe_unused]] const ArrayView<ArrayView<const ImageMappingView, uint16>, Rendering::FrameIndex> outputAttachmentMappings,
		[[maybe_unused]] const ArrayView<const Math::Vector2ui, uint16> outputAttachmentResolutions,
		[[maybe_unused]] const ArrayView<ArrayView<const ImageMappingView, uint16>, Rendering::FrameIndex> outputInputAttachmentMappings,
		[[maybe_unused]] const ArrayView<const Math::Vector2ui, uint16> outputInputAttachmentResolutions,
		[[maybe_unused]] const ArrayView<ArrayView<const ImageMappingView, uint16>, Rendering::FrameIndex> inputAttachmentMappings,
		[[maybe_unused]] const ArrayView<const Math::Vector2ui, uint16> inputAttachmentResolutions,
		[[maybe_unused]] const ArrayView<ArrayView<const Optional<RenderTexture*>, uint16>, Rendering::FrameIndex> outputAttachments
	)
	{
		LogicalDevice& logicalDevice = m_sceneView.GetLogicalDevice();

		if (Ensure(m_descriptorSets[1].IsValid() && m_descriptorSets[2].IsValid() && m_descriptorSets[3].IsValid()))
		{
			const ImageMappingView analyzedDepthMipImageMapping = inputAttachmentMappings[0][0];
			const ImageMappingView depthMinMaxLastImageMapping = inputAttachmentMappings[0][1];
			const Array imageInfos{
				DescriptorSet::ImageInfo{{}, depthMinMaxLastImageMapping, ImageLayout::ShaderReadOnlyOptimal},
				DescriptorSet::ImageInfo{{}, analyzedDepthMipImageMapping, ImageLayout::ShaderReadOnlyOptimal}
			};
			const Array bufferInfos{
				DescriptorSet::BufferInfo{m_sdsmLightInfoBuffer, 0, m_sdsmLightInfoBuffer.GetSize()},
				DescriptorSet::BufferInfo{m_sdsmShadowSamplingInfoBuffer, 0, m_sdsmShadowSamplingInfoBuffer.GetSize()},
				DescriptorSet::BufferInfo{m_shadowGenInfoBuffer, 0, m_shadowGenInfoBuffer.GetSize()},
				DescriptorSet::BufferInfo{m_sdsmShadowMapsIndicesBuffer, 0, m_sdsmShadowMapsIndicesBuffer.GetSize()}
			};
			const Array descriptorUpdates{
				DescriptorSet::UpdateInfo{m_descriptorSets[1], 0, 0, DescriptorType::SampledImage, imageInfos.GetSubView(0, 1)},
				DescriptorSet::UpdateInfo{m_descriptorSets[1], 1, 0, DescriptorType::StorageBuffer, bufferInfos.GetSubView(0, 1)},
				DescriptorSet::UpdateInfo{m_descriptorSets[2], 0, 0, DescriptorType::SampledImage, imageInfos.GetSubView(1, 1)},
				DescriptorSet::UpdateInfo{m_descriptorSets[2], 1, 0, DescriptorType::StorageBuffer, bufferInfos.GetSubView(0, 1)},
				DescriptorSet::UpdateInfo{m_descriptorSets[3], 0, 0, DescriptorType::StorageBuffer, bufferInfos.GetSubView(0, 1)},
				DescriptorSet::UpdateInfo{m_descriptorSets[3], 1, 0, DescriptorType::StorageBuffer, bufferInfos.GetSubView(1, 1)},
				DescriptorSet::UpdateInfo{m_descriptorSets[3], 2, 0, DescriptorType::StorageBuffer, bufferInfos.GetSubView(2, 1)},
				DescriptorSet::UpdateInfo{m_descriptorSets[3], 3, 0, DescriptorType::StorageBuffer, bufferInfos.GetSubView(3, 1)},
			};
			DescriptorSet::Update(logicalDevice, descriptorUpdates.GetView());
		}

		const EnumFlags<PhysicalDeviceFeatures> supportedDeviceFeatures = logicalDevice.GetPhysicalDevice().GetSupportedFeatures();
		const bool supportsLayeredGeometryShader =
			supportedDeviceFeatures.AreAllSet(PhysicalDeviceFeatures::GeometryShader | PhysicalDeviceFeatures::LayeredRendering);
		const uint8 shadowMapMappingArrayLayerCount = supportsLayeredGeometryShader ? MaximumShadowmapCount : 1;

		const ArrayView<const ImageMappingView, uint16> shadowRenderTargetMappings = outputAttachmentMappings[0];
		m_framebuffers.Resize((uint8)shadowRenderTargetMappings.GetSize());
		for (Framebuffer& framebuffer : m_framebuffers)
		{
			framebuffer = Framebuffer(
				m_sceneView.GetLogicalDevice(),
				m_renderPass,
				ArrayView<const ImageMappingView>{shadowRenderTargetMappings[m_framebuffers.GetIteratorIndex(Memory::GetAddressOf(framebuffer))]},
				{ShadowMapSize, ShadowMapSize},
				shadowMapMappingArrayLayerCount
			);
		}
		m_loadedResources = true;
	}

	// Temporary until we refactor buffer creation to use a pool
	constexpr uint32 MaximumVisibleShadowCastingRenderItemCount = 20000;

	void ShadowsStage::OnRenderItemsBecomeVisible(
		const Entity::RenderItemMask& renderItems,
		const Rendering::CommandEncoderView graphicsCommandEncoder,
		PerFrameStagingBuffer& perFrameStagingBuffer

	)
	{
		Rendering::StageCache& stageCache = System::Get<Rendering::Renderer>().GetStageCache();
		const SceneRenderStageIdentifier stageIdentifier = stageCache.FindIdentifier(Guid);

		Entity::RenderItemMask staticMeshes;
		const typename Entity::RenderItemIdentifier::IndexType maximumUsedRenderItemCount =
			m_sceneView.GetSceneChecked()->GetMaximumUsedRenderItemCount();
		Entity::SceneRegistry& sceneRegistry = m_sceneView.GetScene().GetEntitySceneRegistry();
		for (const uint32 renderItemIndex : renderItems.GetSetBitsIterator(0, maximumUsedRenderItemCount))
		{
			const Optional<Entity::HierarchyComponentBase*> pVisibleComponent =
				m_sceneView.GetVisibleRenderItemComponent(Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex));
			Assert(pVisibleComponent.IsValid());
			if (UNLIKELY_ERROR(pVisibleComponent.IsInvalid()))
			{
				continue;
			}

			if (pVisibleComponent->IsStaticMesh(sceneRegistry))
			{
				staticMeshes.Set(Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex));
			}
		}

		if (m_lightGatheringStage.OnRenderItemsBecomeVisible(stageIdentifier, renderItems))
		{
			UpdateLightBuffer(graphicsCommandEncoder, perFrameStagingBuffer);
		}

		if (staticMeshes.AreAnySet())
		{
			m_visibleStaticMeshes.AddRenderItems(
				m_sceneView,
				*m_sceneView.GetSceneChecked(),
				m_sceneView.GetLogicalDevice(),
				stageIdentifier,
				staticMeshes,
				MaximumVisibleShadowCastingRenderItemCount,
				graphicsCommandEncoder,
				perFrameStagingBuffer
			);

			if (m_pBuildAccelerationStructureStage.IsValid())
			{
				m_pBuildAccelerationStructureStage->OnRenderItemsBecomeVisible(staticMeshes, graphicsCommandEncoder, perFrameStagingBuffer);
			}
		}
	}

	void ShadowsStage::OnVisibleRenderItemsReset(
		const Entity::RenderItemMask& renderItems,
		const Rendering::CommandEncoderView graphicsCommandEncoder,
		PerFrameStagingBuffer& perFrameStagingBuffer

	)
	{
		Entity::RenderItemMask staticMeshes;
		Entity::RenderItemMask lights;
		const typename Entity::RenderItemIdentifier::IndexType maximumUsedRenderItemCount =
			m_sceneView.GetSceneChecked()->GetMaximumUsedRenderItemCount();
		Entity::SceneRegistry& sceneRegistry = m_sceneView.GetScene().GetEntitySceneRegistry();
		for (const uint32 renderItemIndex : renderItems.GetSetBitsIterator(0, maximumUsedRenderItemCount))
		{
			const Optional<Entity::HierarchyComponentBase*> pVisibleComponent =
				m_sceneView.GetVisibleRenderItemComponent(Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex));
			Assert(pVisibleComponent.IsValid());
			if (UNLIKELY_ERROR(pVisibleComponent.IsInvalid()))
			{
				continue;
			}

			if (pVisibleComponent->IsStaticMesh(sceneRegistry))
			{
				staticMeshes.Set(Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex));
			}
			if (pVisibleComponent->IsLight())
			{
				lights.Set(Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex));
			}
		}

		Rendering::StageCache& stageCache = System::Get<Rendering::Renderer>().GetStageCache();
		const SceneRenderStageIdentifier stageIdentifier = stageCache.FindIdentifier(Guid);

		if (lights.AreAnySet())
		{
			// TODO: Resetting
			OnRenderItemsBecomeHidden(lights, *m_sceneView.GetSceneChecked(), graphicsCommandEncoder, perFrameStagingBuffer);

			for (const uint32 renderItemIndex : lights.GetSetBitsIterator(0, maximumUsedRenderItemCount))
			{
				const Entity::RenderItemIdentifier renderItemIdentifier = Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex);
				m_sceneView.GetSubmittedRenderItemStageMask(renderItemIdentifier).Clear(stageIdentifier);
				m_sceneView.GetQueuedRenderItemStageMask(renderItemIdentifier).Clear(stageIdentifier);
			}

			OnRenderItemsBecomeVisible(lights, graphicsCommandEncoder, perFrameStagingBuffer);
		}

		if (staticMeshes.AreAnySet())
		{
			m_visibleStaticMeshes.ResetRenderItems(
				m_sceneView,
				*m_sceneView.GetSceneChecked(),
				m_sceneView.GetLogicalDevice(),
				stageIdentifier,
				staticMeshes,
				MaximumVisibleShadowCastingRenderItemCount,
				graphicsCommandEncoder,
				perFrameStagingBuffer
			);

			if (m_pBuildAccelerationStructureStage.IsValid())
			{
				m_pBuildAccelerationStructureStage->OnVisibleRenderItemsReset(staticMeshes, graphicsCommandEncoder, perFrameStagingBuffer);
			}
		}
	}

	void ShadowsStage::OnRenderItemsBecomeHidden(
		const Entity::RenderItemMask& renderItems,
		SceneBase& scene,
		const Rendering::CommandEncoderView graphicsCommandEncoder,
		PerFrameStagingBuffer& perFrameStagingBuffer

	)
	{
		if (m_lightGatheringStage.OnRenderItemsBecomeHidden(renderItems))
		{
			UpdateLightBuffer(graphicsCommandEncoder, perFrameStagingBuffer);
		}

		if (renderItems.AreAnySet())
		{
			m_visibleStaticMeshes
				.RemoveRenderItems(m_sceneView.GetLogicalDevice(), renderItems, scene, graphicsCommandEncoder, perFrameStagingBuffer);
		}

		if (m_pBuildAccelerationStructureStage.IsValid())
		{
			m_pBuildAccelerationStructureStage->OnRenderItemsBecomeHidden(renderItems, scene, graphicsCommandEncoder, perFrameStagingBuffer);
		}
	}

	void ShadowsStage::OnVisibleRenderItemTransformsChanged(
		const Entity::RenderItemMask& renderItems,
		const Rendering::CommandEncoderView graphicsCommandEncoder,
		PerFrameStagingBuffer& perFrameStagingBuffer

	)
	{
		bool encounteredLights = false;
		Entity::RenderItemMask meshes;

		const typename Entity::RenderItemIdentifier::IndexType maximumUsedRenderItemCount =
			m_sceneView.GetSceneChecked()->GetMaximumUsedRenderItemCount();
		for (const uint32 renderItemIndex : renderItems.GetSetBitsIterator(0, maximumUsedRenderItemCount))
		{
			const Optional<Entity::HierarchyComponentBase*> pVisibleComponent =
				m_sceneView.GetVisibleRenderItemComponent(Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex));
			Assert(pVisibleComponent.IsValid());
			if (UNLIKELY_ERROR(pVisibleComponent.IsInvalid()))
			{
				continue;
			}

			encounteredLights |= pVisibleComponent->IsLight();
			if (!pVisibleComponent->IsLight())
			{
				meshes.Set(Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex));
			}
		}

		if (encounteredLights)
		{
			m_lightGatheringStage.OnVisibleRenderItemTransformsChanged(renderItems);
			UpdateLightBuffer(graphicsCommandEncoder, perFrameStagingBuffer);
		}

		if (meshes.AreAnySet())
		{
			if (m_pBuildAccelerationStructureStage.IsValid())
			{
				m_pBuildAccelerationStructureStage->OnVisibleRenderItemTransformsChanged(meshes, graphicsCommandEncoder, perFrameStagingBuffer);
			}
		}
	}

	Threading::JobBatch ShadowsStage::LoadRenderItemsResources(const Entity::RenderItemMask& renderItems)
	{
		const typename Entity::RenderItemIdentifier::IndexType maximumUsedRenderItemCount =
			m_sceneView.GetSceneChecked()->GetMaximumUsedRenderItemCount();
		Entity::SceneRegistry& sceneRegistry = m_sceneView.GetSceneChecked()->GetEntitySceneRegistry();
		for (const uint32 renderItemIndex : renderItems.GetSetBitsIterator(0, maximumUsedRenderItemCount))
		{
			const Optional<Entity::HierarchyComponentBase*> pVisibleComponent =
				m_sceneView.GetVisibleRenderItemComponent(Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex));
			Assert(pVisibleComponent.IsValid());
			if (UNLIKELY_ERROR(pVisibleComponent.IsInvalid()))
			{
				continue;
			}

			if (pVisibleComponent->IsStaticMesh(sceneRegistry))
			{
				return m_visibleStaticMeshes.TryLoadRenderItemResources(m_sceneView.GetLogicalDevice(), *pVisibleComponent, sceneRegistry);
			}
		}
		return {};
	}

	void ShadowsStage::OnSceneUnloaded()
	{
		m_lightGatheringStage.OnSceneUnloaded();
		m_visibleStaticMeshes.OnSceneUnloaded(m_sceneView.GetLogicalDevice(), *m_sceneView.GetSceneChecked());
	}

	void ShadowsStage::OnActiveCameraPropertiesChanged(
		[[maybe_unused]] const Rendering::CommandEncoderView graphicsCommandEncoder, PerFrameStagingBuffer& perFrameStagingBuffer

	)
	{
		m_lightGatheringStage.OnActiveCameraPropertiesChanged();
		UpdateLightBuffer(graphicsCommandEncoder, perFrameStagingBuffer);
	}

	void ShadowsStage::UpdateLightBuffer(
		const Rendering::CommandEncoderView graphicsCommandEncoder, PerFrameStagingBuffer& perFrameStagingBuffer

	)
	{
		Array<DescriptorSet::UpdateInfo, 1> descriptorUpdates;
		Array<DescriptorSet::BufferInfo, 1> bufferInfo;

		{
			LogicalDevice& logicalDevice = m_logicalDevice;

#if ENABLE_SAMPLE_DISTRIBUTION_SHADOW_MAPS
			const ArrayView<const uint32, ShadowMapIndexType> parallelShadowMapsIndices = m_lightGatheringStage.GetParallelShadowMapsIndices();
			if (parallelShadowMapsIndices.HasElements())
			{
				perFrameStagingBuffer.CopyToBuffer(
					logicalDevice,
					logicalDevice.GetCommandQueue(QueueFamily::Graphics),
					graphicsCommandEncoder,
					ConstByteView(parallelShadowMapsIndices),
					m_sdsmShadowMapsIndicesBuffer
				);
			}
#endif

			const ArrayView<const LightGatheringStage::ShadowInfo, ShadowMapIndexType> visibleShadowCastingLights =
				m_lightGatheringStage.GetVisibleShadowCastingLights();
			const uint32 lightCount = visibleShadowCastingLights.GetSize();

			const LightGatheringStage::Header header = {lightCount};
			PerFrameStagingBuffer::BatchCopyContext batchCopyContext =
				perFrameStagingBuffer.BeginBatchCopyToBuffer(sizeof(header) + visibleShadowCastingLights.GetDataSize(), m_shadowGenInfoBuffer);
			const CommandQueueView graphicsCommandQueue = m_logicalDevice.GetCommandQueue(QueueFamily::Graphics);
			perFrameStagingBuffer.BatchCopyToBuffer(logicalDevice, batchCopyContext, graphicsCommandQueue, ConstByteView::Make(header), 0);
			perFrameStagingBuffer.BatchCopyToBuffer(
				logicalDevice,
				batchCopyContext,
				graphicsCommandQueue,
				ConstByteView(visibleShadowCastingLights),
				sizeof(header)
			);

			perFrameStagingBuffer.EndBatchCopyToBuffer(batchCopyContext, graphicsCommandEncoder);

			bufferInfo[0] =
				DescriptorSet::BufferInfo{m_shadowGenInfoBuffer, 0, sizeof(LightGatheringStage::Header) + visibleShadowCastingLights.GetDataSize()};

			descriptorUpdates[0] = DescriptorSet::UpdateInfo{
				m_descriptorSets[0],
				0,
				0,
				DescriptorType::StorageBuffer,
				ArrayView<const DescriptorSet::BufferInfo>(bufferInfo[0])
			};
		}

		if (Ensure(m_descriptorSets[0].IsValid()))
		{
			Threading::UniqueLock lock(m_textureLoadMutex);
			DescriptorSet::Update(m_sceneView.GetLogicalDevice(), descriptorUpdates);
		}
	}

	bool ShadowsStage::ShouldRecordCommands() const
	{
		switch (m_state)
		{
			case State::Disabled:
			case State::Raytraced:
				return false;
			case State::Rasterized:
			{
				return m_framebuffers.GetView().All(
								 [](const FramebufferView framebuffer)
								 {
									 return framebuffer.IsValid();
								 }
							 ) &&
				       m_sceneView.HasActiveCamera() &&
				       (m_lightGatheringStage.HasVisibleShadowCastingLights() && m_visibleStaticMeshes.HasVisibleItems()) && m_pipeline.IsValid() &&
				       m_sdsmPhase1Pipeline.IsValid() && m_sdsmPhase2Pipeline.IsValid() && m_sdsmPhase3Pipeline.IsValid() && m_loadedResources;
			}
		}
		ExpectUnreachable();
	}

	void ShadowsStage::RecordCommands(const CommandEncoderView graphicsCommandEncoder)
	{
#if RENDERER_OBJECT_DEBUG_NAMES
		graphicsCommandEncoder.SetDebugName(m_sceneView.GetLogicalDevice(), "Shadows");
		const DebugMarker debugMarker{graphicsCommandEncoder, m_sceneView.GetLogicalDevice(), "Shadows", "#00FF00"_color};
#endif

#if ENABLE_SAMPLE_DISTRIBUTION_SHADOW_MAPS
		AnalyzeDepthBufferToSetupParallelSplitShadowMaps(graphicsCommandEncoder);
#endif

		constexpr Array<ClearValue, 1> clearValues = {DepthStencilValue{1.f, 0}};

		const DescriptorSetView transformBufferDescriptorSet = m_sceneView.GetTransformBufferDescriptorSet();

		if (m_logicalDevice.GetPhysicalDevice().GetSupportedFeatures().AreAllSet(
					PhysicalDeviceFeatures::GeometryShader | PhysicalDeviceFeatures::LayeredRendering
				))
		{
			const VisibleRenderItems::VisibleInstanceGroups::ConstDynamicView instanceGroups = m_visibleStaticMeshes.GetVisibleItems();
			Rendering::RenderCommandEncoder renderCommandEncoder = graphicsCommandEncoder.BeginRenderPass(
				m_logicalDevice,
				m_renderPass,
				m_framebuffers[0],
				Math::Rectangleui{Math::Zero, {ShadowMapSize, ShadowMapSize}},
				clearValues,
				instanceGroups.GetSize()
			);
			renderCommandEncoder.BindPipeline(m_pipeline);

			renderCommandEncoder
				.BindDescriptorSets(m_pipeline, Array<const DescriptorSetView, 2>{transformBufferDescriptorSet, m_descriptorSets[0]});

			for (const Optional<VisibleRenderItems::InstanceGroup*> pInstanceGroup : instanceGroups)
			{
				if (pInstanceGroup == nullptr)
				{
					continue;
				}

				if (pInstanceGroup->m_instanceBuffer.GetInstanceCount() > 0)
				{
					const VisibleStaticMeshes::InstanceGroup& instanceGroup = static_cast<const VisibleStaticMeshes::InstanceGroup&>(*pInstanceGroup);
					m_pipeline.Draw(
						instanceGroup.m_instanceBuffer.GetFirstInstanceIndex(),
						instanceGroup.m_instanceBuffer.GetInstanceCount(),
						instanceGroup.m_renderMeshView,
						instanceGroup.m_instanceBuffer.GetBuffer(),
						renderCommandEncoder
					);
				}
			}
		}
		else
		{
			const ArrayView<const LightGatheringStage::ShadowInfo, ShadowMapIndexType> visibleShadowCastingLights =
				m_lightGatheringStage.GetVisibleShadowCastingLights();
			for (const LightGatheringStage::ShadowInfo& lightInfo : visibleShadowCastingLights)
			{
				const ShadowMapIndexType shadowmapIndex = visibleShadowCastingLights.GetIteratorIndex(Memory::GetAddressOf(lightInfo));

				Rendering::RenderCommandEncoder renderCommandEncoder = graphicsCommandEncoder.BeginRenderPass(
					m_logicalDevice,
					m_renderPass,
					m_framebuffers[shadowmapIndex],
					Math::Rectangleui{Math::Zero, {ShadowMapSize, ShadowMapSize}},
					clearValues,
					1
				);

				renderCommandEncoder.BindDescriptorSets(
					m_pipeline,
					Array<const DescriptorSetView, 2>{transformBufferDescriptorSet, m_descriptorSets[0]},
					m_pipeline.GetFirstDescriptorSetIndex()
				);

#if RENDERER_OBJECT_DEBUG_NAMES
				const RenderDebugMarker debugMarker2{renderCommandEncoder, m_sceneView.GetLogicalDevice(), "ShadowMap", "#FF0000"_color};
#endif

				renderCommandEncoder.BindPipeline(m_pipeline);

				const EnumFlags<PhysicalDeviceFeatures> supportedDeviceFeatures = m_logicalDevice.GetPhysicalDevice().GetSupportedFeatures();
				const bool supportsLayeredGeometryShader =
					supportedDeviceFeatures.AreAllSet(PhysicalDeviceFeatures::GeometryShader | PhysicalDeviceFeatures::LayeredRendering);
				if (!supportsLayeredGeometryShader)
				{
					const WithoutGeometryShader::ShadowConstants constants = {WithoutGeometryShader::ShadowConstants::VertexConstants{shadowmapIndex}
					};

					m_pipeline.PushConstants(m_logicalDevice, renderCommandEncoder, WithoutGeometryShader::ShadowPushConstantRanges, constants);
				}

				const VisibleRenderItems::VisibleInstanceGroups::ConstDynamicView instanceGroups = m_visibleStaticMeshes.GetVisibleItems();
				for (const Optional<VisibleRenderItems::InstanceGroup*> pInstanceGroup : instanceGroups)
				{
					if (pInstanceGroup == nullptr)
					{
						continue;
					}

					if (pInstanceGroup->m_instanceBuffer.GetInstanceCount() > 0)
					{
						const VisibleStaticMeshes::InstanceGroup& instanceGroup = static_cast<const VisibleStaticMeshes::InstanceGroup&>(*pInstanceGroup
						);
						m_pipeline.Draw(
							instanceGroup.m_instanceBuffer.GetFirstInstanceIndex(),
							instanceGroup.m_instanceBuffer.GetInstanceCount(),
							instanceGroup.m_renderMeshView,
							instanceGroup.m_instanceBuffer.GetBuffer(),
							renderCommandEncoder
						);
					}
				}
			}
		}
	}

#if ENABLE_SAMPLE_DISTRIBUTION_SHADOW_MAPS

	void ShadowsStage::AnalyzeDepthBufferToSetupParallelSplitShadowMaps(const CommandEncoderView graphicsCommandEncoder)
	{
#if RENDERER_OBJECT_DEBUG_NAMES
		const DebugMarker debugMarker{graphicsCommandEncoder, m_sceneView.GetLogicalDevice(), "SetupPSSMSplits", "#FF0000"_color};
#endif

		// TODO check if all of these barriers are necessary

		const DirectionalLightIndexType numDirectionalShadowingCastingLight = m_lightGatheringStage.GetDirectionalShadowingCastingLightCount();
		if (numDirectionalShadowingCastingLight > 0)
		{
			const ArrayView<const VisibleLight> visibleDirectionalLights = m_lightGatheringStage.GetVisibleLights(LightTypes::DirectionalLight);
			for (DirectionalLightIndexType i = 0; i < numDirectionalShadowingCastingLight; ++i)
			{
				m_sdsmLightInfos[i].ViewMatrix = Math::Matrix4x4f(Math::Identity);

				for (uint32 j = 0; j < LightGatheringStage::MaximumDirectionalLightCascadeCount; ++j)
				{
					m_sdsmLightInfos[i].BoundMin[j] = Math::Vector4i(0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0);
					m_sdsmLightInfos[i].BoundMax[j] = Math::Vector4i(0x80000000, 0x80000000, 0x80000000, 0);
				}

				m_sdsmLightInfos[i].CascadeSplits = Math::Vector4f(Math::Zero);
				m_sdsmLightInfos[i].LightDirection = visibleDirectionalLights[i].light->GetWorldForwardDirection();

				// TODO expose these paramaters
				m_sdsmLightInfos[i].PSSMLambda = 1.0;
				m_sdsmLightInfos[i].MaxShadowRange = 10000.0;
			}

			{
				const BlitCommandEncoder blitCommandEncoder = graphicsCommandEncoder.BeginBlit();
				Optional<StagingBuffer> stagingBuffer;
				blitCommandEncoder.RecordCopyDataToBuffer(
					m_logicalDevice,
					QueueFamily::Graphics,
					Array<const DataToBufferBatch, 1>{DataToBufferBatch{
						m_sdsmLightInfoBuffer,
						Array<const DataToBuffer, 1>{DataToBuffer{0, ConstByteView(m_sdsmLightInfos.GetDynamicView())}}
					}},
					m_sdsmStagingBuffer
				);
			}

			{
				const ComputeCommandEncoder computeCommandEncoder = graphicsCommandEncoder.BeginCompute(m_logicalDevice, 1);
				computeCommandEncoder.BindPipeline(m_sdsmPhase1Pipeline);

				Math::Vector2ui screenResolution = (Math::Vector2ui)(Math::Vector2f(m_sceneView.GetRenderResolution()) * UpscalingFactor);

				screenResolution = Math::Max(screenResolution >> Math::Vector2ui{1}, Math::Vector2ui{1u});
				Rendering::MipMask mipMask = Rendering::MipMask::FromSizeAllToLargest(screenResolution);
				const uint16 depthMinMaxMipCount = mipMask.GetSize();
				for (uint16 i = 1; i < depthMinMaxMipCount; ++i)
				{
					screenResolution /= 2;
				}

				m_sdsmPhase1Pipeline.Compute(
					m_logicalDevice,
					Array<const DescriptorSetView, 2>(m_sceneView.GetMatrices().GetDescriptorSet(), m_descriptorSets[1].AtomicLoad()),
					computeCommandEncoder,
					numDirectionalShadowingCastingLight,
					screenResolution
				);
			}

			{
				const Array<Rendering::BufferMemoryBarrier, 1> barriers{Rendering::BufferMemoryBarrier{
					Rendering::AccessFlags::ShaderReadWrite,
					Rendering::AccessFlags::ShaderReadWrite,
					m_sdsmLightInfoBuffer,
					0,
					m_sdsmLightInfoBuffer.GetSize()
				}};

				graphicsCommandEncoder.RecordPipelineBarrier(
					Rendering::PipelineStageFlags::ComputeShader,
					Rendering::PipelineStageFlags::ComputeShader,
					{},
					barriers.GetView()
				);
			}

			{
				const ComputeCommandEncoder computeCommandEncoder =
					graphicsCommandEncoder.BeginCompute(m_logicalDevice, numDirectionalShadowingCastingLight);
				computeCommandEncoder.BindPipeline(m_sdsmPhase2Pipeline);

				const Math::Vector2ui renderResolution =
					(Math::Vector2ui)(Math::Vector2f(m_sceneView.GetDrawer().GetFullRenderResolution()) * UpscalingFactor);
				Math::Vector2ui analysedDepthMipRes = renderResolution;
				analysedDepthMipRes = Math::Max(analysedDepthMipRes >> Math::Vector2ui{1}, Math::Vector2ui{1u});
				analysedDepthMipRes = Math::Max(analysedDepthMipRes >> Math::Vector2ui{1}, Math::Vector2ui{1u});

				for (DirectionalLightIndexType iLight = 0; iLight < numDirectionalShadowingCastingLight; ++iLight)
				{
					m_sdsmPhase2Pipeline.Compute(
						m_logicalDevice,
						Array<const DescriptorSetView, 2>(m_sceneView.GetMatrices().GetDescriptorSet(), m_descriptorSets[2].AtomicLoad()),
						computeCommandEncoder,
						iLight,
						analysedDepthMipRes
					);
				}
			}

			{
				const Array<Rendering::BufferMemoryBarrier, 1> barriers{Rendering::BufferMemoryBarrier{
					Rendering::AccessFlags::ShaderReadWrite,
					Rendering::AccessFlags::ShaderRead,
					m_sdsmLightInfoBuffer,
					0,
					m_sdsmLightInfoBuffer.GetSize()
				}};

				graphicsCommandEncoder.RecordPipelineBarrier(
					Rendering::PipelineStageFlags::ComputeShader,
					Rendering::PipelineStageFlags::ComputeShader,
					{},
					barriers.GetView()
				);
			}
			{
				const Array<Rendering::BufferMemoryBarrier, 1> barriers{Rendering::BufferMemoryBarrier{
					Rendering::AccessFlags::ShaderRead,
					Rendering::AccessFlags::ShaderWrite,
					m_sdsmShadowSamplingInfoBuffer,
					0,
					m_sdsmShadowSamplingInfoBuffer.GetSize()
				}};

				graphicsCommandEncoder.RecordPipelineBarrier(
					Rendering::PipelineStageFlags::FragmentShader,
					Rendering::PipelineStageFlags::ComputeShader,
					{},
					barriers.GetView()
				);
			}
			{
				const Array<Rendering::BufferMemoryBarrier, 1> barriers{Rendering::BufferMemoryBarrier{
					Rendering::AccessFlags::ShaderRead,
					Rendering::AccessFlags::ShaderWrite,
					m_shadowGenInfoBuffer,
					0,
					m_shadowGenInfoBuffer.GetSize()
				}};

				graphicsCommandEncoder.RecordPipelineBarrier(
					Rendering::PipelineStageFlags::VertexShader | Rendering::PipelineStageFlags::GeometryShader,
					Rendering::PipelineStageFlags::ComputeShader,
					{},
					barriers.GetView()
				);
			}

			{
				const ComputeCommandEncoder computeCommandEncoder = graphicsCommandEncoder.BeginCompute(m_logicalDevice, 0);
				computeCommandEncoder.BindPipeline(m_sdsmPhase3Pipeline);
				m_sdsmPhase3Pipeline.Compute(
					ArrayView<const DescriptorSetView>(m_descriptorSets[3].AtomicLoad()),
					computeCommandEncoder,
					numDirectionalShadowingCastingLight
				);
			}

			{
				const Array<Rendering::BufferMemoryBarrier, 1> barriers{Rendering::BufferMemoryBarrier{
					Rendering::AccessFlags::ShaderWrite,
					Rendering::AccessFlags::ShaderRead,
					m_shadowGenInfoBuffer,
					0,
					m_shadowGenInfoBuffer.GetSize()
				}};

				graphicsCommandEncoder.RecordPipelineBarrier(
					Rendering::PipelineStageFlags::ComputeShader,
					Rendering::PipelineStageFlags::VertexShader | Rendering::PipelineStageFlags::GeometryShader,
					{},
					barriers.GetView()
				);
			}
			{
				const Array<Rendering::BufferMemoryBarrier, 1> barriers{Rendering::BufferMemoryBarrier{
					Rendering::AccessFlags::ShaderWrite,
					Rendering::AccessFlags::ShaderRead,
					m_sdsmShadowSamplingInfoBuffer,
					0,
					m_sdsmShadowSamplingInfoBuffer.GetSize()
				}};

				graphicsCommandEncoder.RecordPipelineBarrier(
					Rendering::PipelineStageFlags::ComputeShader,
					Rendering::PipelineStageFlags::VertexShader, // and GeometryShader
					{},
					barriers.GetView()
				);
			}
		}
	}
#endif
}
