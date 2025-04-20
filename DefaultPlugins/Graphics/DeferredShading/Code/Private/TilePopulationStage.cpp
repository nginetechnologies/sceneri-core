#include "TilePopulationStage.h"
#include "VisibleLight.h"
#include "LightGatheringStage.h"
#include "PBRLightingStage.h"
#include "LightInfo.h"

#include <Common/Threading/Jobs/AsyncJob.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/Math/Log2.h>
#include <Common/Math/Vector2/Mod.h>
#include <Common/Math/Vector2/Sign.h>
#include <Common/Memory/AddressOf.h>

#include <Engine/Threading/JobRunnerThread.h>

#include <Common/System/Query.h>
#include <Engine/Threading/JobManager.h>
#include <Engine/Entity/CameraComponent.h>
#include <Engine/Entity/Lights/SpotLightComponent.h>
#include <Engine/Entity/Lights/PointLightComponent.h>
#include <Engine/Entity/Lights/DirectionalLightComponent.h>
#include <Engine/Entity/Lights/EnvironmentLightComponent.h>
#include <Engine/Scene/Scene.h>

#include <Renderer/Commands/SingleUseCommandBuffer.h>
#include <Renderer/Commands/ComputeCommandEncoder.h>
#include <Renderer/Commands/BlitCommandEncoder.h>
#include <Renderer/Scene/SceneView.h>
#include <Renderer/Scene/SceneData.h>
#include <Renderer/Renderer.h>
#include <Renderer/Stages/StartFrameStage.h>
#include <Renderer/Stages/PerFrameStagingBuffer.h>
#include <Renderer/Wrappers/AttachmentDescription.h>
#include <Renderer/Wrappers/AttachmentReference.h>
#include <Renderer/Wrappers/SubpassDependency.h>
#include <Renderer/Wrappers/CompareOperation.h>
#include <Renderer/RenderOutput/RenderOutput.h>
#include <Renderer/Assets/Texture/RenderTexture.h>
#include <Renderer/Buffers/DataToBufferBatch.h>
#include <DeferredShading/FSR/fsr_settings.h>

namespace ngine::Rendering
{
	[[nodiscard]] Math::Vector2ui TilePopulationStage::CalculateTileSize(const Math::Vector2ui renderResolution)
	{
		return (renderResolution / TilePopulationPipeline::TileSize) +
		       Math::Sign(Math::Mod(renderResolution, Math::Vector2ui{TilePopulationPipeline::TileSize}));
	}

	TilePopulationStage::TilePopulationStage(SceneView& sceneView)
		: RenderItemStage(sceneView.GetLogicalDevice(), Threading::JobPriority::Draw)
		, m_sceneView(sceneView)
		, m_lightGatheringStage(sceneView)
		, m_tilePopulationPipeline(
				m_sceneView.GetLogicalDevice(), m_sceneView.GetLogicalDevice().GetShaderCache(), m_sceneView.GetMatrices().GetDescriptorSetLayout()
			)
		, m_tiledImageSampler(
				m_sceneView.GetLogicalDevice(),
				AddressMode::Repeat,
				FilterMode::Nearest,
				CompareOperation::AlwaysSucceed,
				Math::Range<int16>::MakeStartToEnd(0, 1)
			)
		, m_lightStorageBuffers{
				StorageBuffer{
					m_sceneView.GetLogicalDevice(),
					m_sceneView.GetLogicalDevice().GetPhysicalDevice(),
					m_sceneView.GetLogicalDevice().GetDeviceMemoryPool(),
					PointLightBufferSize
				},
				StorageBuffer{
					m_sceneView.GetLogicalDevice(),
					m_sceneView.GetLogicalDevice().GetPhysicalDevice(),
					m_sceneView.GetLogicalDevice().GetDeviceMemoryPool(),
					SpotLightBufferSize
				},
				StorageBuffer{
					m_sceneView.GetLogicalDevice(),
					m_sceneView.GetLogicalDevice().GetPhysicalDevice(),
					m_sceneView.GetLogicalDevice().GetDeviceMemoryPool(),
					DirectionalLightBufferSize
				}
			}
	{
		// Copy header into directional light if necessary
		for (LightTypes lightType = LightTypes::First; lightType <= LightTypes::LastRealLight; lightType = LightTypes((uint8)lightType + 1))
		{
			const uint8 lightTypeIndex = (uint8)lightType;
			if (LightHeaderSizes[lightTypeIndex] != 0)
			{
				LightHeader& header = m_lightHeaders[lightType];
				header = {0};

				Threading::EngineJobRunnerThread& thread = *Threading::EngineJobRunnerThread::GetCurrent();
				const Rendering::CommandPoolView commandPool =
					thread.GetRenderData().GetCommandPool(m_logicalDevice.GetIdentifier(), Rendering::QueueFamily::Transfer);
				Rendering::SingleUseCommandBuffer
					commandBuffer(m_logicalDevice, commandPool, thread, Rendering::QueueFamily::Transfer, Threading::JobPriority::CreateRenderMesh);

				const CommandEncoderView commandEncoder = commandBuffer;
				const BlitCommandEncoder blitCommandEncoder = commandEncoder.BeginBlit();
				Optional<StagingBuffer> stagingBuffer;
				blitCommandEncoder.RecordCopyDataToBuffer(
					m_logicalDevice,
					QueueFamily::Graphics,
					Array<const DataToBufferBatch, 1>{DataToBufferBatch{
						m_lightStorageBuffers[lightTypeIndex],
						Array<const DataToBuffer, 1>{DataToBuffer{0, ConstByteView::Make(header)}}
					}},
					stagingBuffer
				);
				if (stagingBuffer.IsValid())
				{
					commandBuffer.OnFinished = [&logicalDevice = m_logicalDevice, stagingBuffer = Move(*stagingBuffer)]() mutable
					{
						stagingBuffer.Destroy(logicalDevice, logicalDevice.GetDeviceMemoryPool());
					};
				}
			}
		}

		const SceneRenderStageIdentifier stageIdentifier = System::Get<Rendering::Renderer>().GetStageCache().FindIdentifier(Guid);
		m_sceneView.RegisterRenderItemStage(stageIdentifier, *this);
		m_sceneView.SetStageDependentOnCameraProperties(stageIdentifier);
	}

	TilePopulationStage::~TilePopulationStage()
	{
		m_tilePopulationPipeline.Destroy(m_sceneView.GetLogicalDevice());

		if (m_pDescriptorSetLoadingThread != nullptr)
		{
			Threading::EngineJobRunnerThread& previousEngineThread = static_cast<Threading::EngineJobRunnerThread&>(*m_pDescriptorSetLoadingThread
			);
			previousEngineThread.GetRenderData().DestroyDescriptorSet(m_logicalDevice.GetIdentifier(), Move(m_tileDescriptorSet));
		}

		m_tiledImageSampler.Destroy(m_sceneView.GetLogicalDevice());

		for (StorageBuffer& lightBuffer : m_lightStorageBuffers)
		{
			lightBuffer.Destroy(m_sceneView.GetLogicalDevice(), m_sceneView.GetLogicalDevice().GetDeviceMemoryPool());
		}

		Rendering::StageCache& stageCache = System::Get<Rendering::Renderer>().GetStageCache();
		const SceneRenderStageIdentifier stageIdentifier = stageCache.FindIdentifier(Guid);
		m_sceneView.DeregisterRenderItemStage(stageIdentifier);
	}

	void TilePopulationStage::OnComputePassAttachmentsLoaded(
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
	)
	{
		m_tiledImageMappingView = outputAttachmentMappings[0][0];
		Threading::EngineJobRunnerThread& engineThread = *Threading::EngineJobRunnerThread::GetCurrent();
		{
			Array<DescriptorSet, 1> descriptorSets;
			[[maybe_unused]] const bool allocatedDescriptorSets = engineThread.GetRenderData()
			                                                        .GetDescriptorPool(m_sceneView.GetLogicalDevice().GetIdentifier())
			                                                        .AllocateDescriptorSets(
																																m_sceneView.GetLogicalDevice(),
																																Array<const DescriptorSetLayoutView, 1>(m_tilePopulationPipeline),
																																descriptorSets
																															);
			Assert(allocatedDescriptorSets);
			if (LIKELY(allocatedDescriptorSets))
			{
				Threading::JobRunnerThread* pPreviousDescriptorSetLoadingThread = m_pDescriptorSetLoadingThread;

				if (descriptorSets[0].IsValid())
				{
					Threading::UniqueLock lock(m_descriptorMutex);
					PopulateTileDescriptorSet(descriptorSets[0]);
				}

				m_tileDescriptorSet.AtomicSwap(descriptorSets[0]);
				m_pDescriptorSetLoadingThread = &engineThread;

				if (pPreviousDescriptorSetLoadingThread != nullptr)
				{
					Threading::EngineJobRunnerThread& previousEngineThread =
						static_cast<Threading::EngineJobRunnerThread&>(*pPreviousDescriptorSetLoadingThread);
					previousEngineThread.GetRenderData().DestroyDescriptorSet(m_logicalDevice.GetIdentifier(), Move(descriptorSets[0]));
				}
			}
		}
	}

	void TilePopulationStage::PopulateTileDescriptorSet(const Rendering::DescriptorSetView descriptorSet)
	{
		Array<DescriptorSet::ImageInfo, 1> imageInfo{
			DescriptorSet::ImageInfo{m_tiledImageSampler, m_tiledImageMappingView, ImageLayout::General}
		};
		Array<DescriptorSet::BufferInfo, (uint8)LightTypes::Count - 1> bufferInfo;
		Array<DescriptorSet::UpdateInfo, imageInfo.GetSize() + bufferInfo.GetSize()> descriptorUpdates;

		{
			descriptorUpdates[0] = DescriptorSet::UpdateInfo{
				descriptorSet,
				0,
				0,
				DescriptorType::StorageImage,
				ArrayView<const DescriptorSet::ImageInfo>(imageInfo[0])
			};
		}

		for (StorageBuffer& lightBuffer : m_lightStorageBuffers)
		{
			const uint8 index = m_lightStorageBuffers.GetIteratorIndex(Memory::GetAddressOf(lightBuffer));
			bufferInfo[index] = {
				lightBuffer,
				0,
				LightHeaderSizes[index] +
					Math::Max(
						LightStructSizes[index] * Math::Min(m_visibleLights[index].GetSize(), (uint32)MaximumLightCounts[index]),
						LightStructSizes[index]
					)
			};

			descriptorUpdates[1 + index] = DescriptorSet::UpdateInfo{
				descriptorSet,
				DescriptorSetView::BindingIndexType(1 + index),
				0,
				DescriptorType::StorageBuffer,
				ArrayView<const DescriptorSet::BufferInfo>(bufferInfo[index])
			};
		}

		DescriptorSet::Update(m_sceneView.GetLogicalDevice(), descriptorUpdates);
	}

	void TilePopulationStage::OnRenderItemsBecomeVisible(
		const Entity::RenderItemMask& renderItems,
		[[maybe_unused]] const Rendering::CommandEncoderView graphicsCommandEncoder,
		PerFrameStagingBuffer& perFrameStagingBuffer
	)
	{
		Rendering::StageCache& stageCache = System::Get<Rendering::Renderer>().GetStageCache();
		const SceneRenderStageIdentifier stageIdentifier = stageCache.FindIdentifier(Guid);

		const typename Entity::RenderItemIdentifier::IndexType maximumUsedRenderItemCount =
			m_sceneView.GetSceneChecked()->GetMaximumUsedRenderItemCount();
		for (const uint32 renderItemIndex : renderItems.GetSetBitsIterator(0, maximumUsedRenderItemCount))
		{
			const Entity::RenderItemIdentifier renderItemIdentifier = Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex);
			const Optional<Entity::HierarchyComponentBase*> pVisibleComponent = m_sceneView.GetVisibleRenderItemComponent(renderItemIdentifier);
			Assert(pVisibleComponent.IsValid());
			if (UNLIKELY_ERROR(pVisibleComponent.IsInvalid()))
			{
				continue;
			}

			Assert(!m_sceneView.GetSubmittedRenderItemStageMask(renderItemIdentifier).IsSet(stageIdentifier));

			Assert(pVisibleComponent->IsLight());
			const Entity::LightSourceComponent& lightComponent = pVisibleComponent->AsExpected<Entity::LightSourceComponent>();
			const LightTypes lightType = LightGatheringStage::GetLightType(lightComponent);

			m_visibleLights[(uint8)lightType].EmplaceBack(lightComponent);

			m_sceneView.GetSubmittedRenderItemStageMask(renderItemIdentifier).Set(stageIdentifier);
		}

		UpdateLightBuffer(graphicsCommandEncoder, perFrameStagingBuffer);
		if (m_tileDescriptorSet.IsValid())
		{
			UpdateLightBufferDescriptorSet();
		}
		else
		{
			m_shouldUpdateLightBuffer = true;
		}

		if (m_pPBRLightingStage.IsValid())
		{
			m_pPBRLightingStage->OnRenderItemsBecomeVisible(renderItems);
		}
	}

	void TilePopulationStage::OnVisibleRenderItemsReset(
		const Entity::RenderItemMask& renderItems,
		const Rendering::CommandEncoderView graphicsCommandEncoder,
		PerFrameStagingBuffer& perFrameStagingBuffer

	)
	{
		// TODO: Resetting
		OnRenderItemsBecomeHidden(renderItems, *m_sceneView.GetSceneChecked(), graphicsCommandEncoder, perFrameStagingBuffer);

		Rendering::StageCache& stageCache = System::Get<Rendering::Renderer>().GetStageCache();
		const SceneRenderStageIdentifier stageIdentifier = stageCache.FindIdentifier(Guid);

		const typename Entity::RenderItemIdentifier::IndexType maximumUsedRenderItemCount =
			m_sceneView.GetSceneChecked()->GetMaximumUsedRenderItemCount();
		for (const uint32 renderItemIndex : renderItems.GetSetBitsIterator(0, maximumUsedRenderItemCount))
		{
			const Entity::RenderItemIdentifier renderItemIdentifier = Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex);
			m_sceneView.GetSubmittedRenderItemStageMask(renderItemIdentifier).Clear(stageIdentifier);
			m_sceneView.GetQueuedRenderItemStageMask(renderItemIdentifier).Clear(stageIdentifier);
		}

		OnRenderItemsBecomeVisible(renderItems, graphicsCommandEncoder, perFrameStagingBuffer);

		if (m_pPBRLightingStage.IsValid())
		{
			m_pPBRLightingStage->OnVisibleRenderItemsReset(renderItems);
		}
	}

	void TilePopulationStage::OnRenderItemsBecomeHidden(
		const Entity::RenderItemMask& renderItems,
		SceneBase&,
		[[maybe_unused]] const Rendering::CommandEncoderView graphicsCommandEncoder,
		PerFrameStagingBuffer& perFrameStagingBuffer
	)
	{
		const typename Entity::RenderItemIdentifier::IndexType maximumUsedRenderItemCount =
			m_sceneView.GetSceneChecked()->GetMaximumUsedRenderItemCount();
		for (const uint32 renderItemIndex : renderItems.GetSetBitsIterator(0, maximumUsedRenderItemCount))
		{
			for (auto& typeVisibleLights : m_visibleLights)
			{
				if (typeVisibleLights.RemoveFirstOccurrencePredicate(
							[renderItemIndex](const Entity::LightSourceComponent& component) -> ErasePredicateResult
							{
								return component.GetRenderItemIdentifier().GetFirstValidIndex() == renderItemIndex ? ErasePredicateResult::Remove
					                                                                                         : ErasePredicateResult::Continue;
							}
						))
				{
					break;
				}
			}
		}

		UpdateLightBuffer(graphicsCommandEncoder, perFrameStagingBuffer);
		if (m_tileDescriptorSet.IsValid())
		{
			UpdateLightBufferDescriptorSet();
		}
		else
		{
			m_shouldUpdateLightBuffer = true;
		}

		if (m_pPBRLightingStage.IsValid())
		{
			m_pPBRLightingStage->OnRenderItemsBecomeHidden(renderItems);
		}
	}

	void TilePopulationStage::OnVisibleRenderItemTransformsChanged(
		const Entity::RenderItemMask& renderItems,
		const Rendering::CommandEncoderView graphicsCommandEncoder,
		PerFrameStagingBuffer& perFrameStagingBuffer

	)
	{
		UpdateLightBuffer(graphicsCommandEncoder, perFrameStagingBuffer);
		if (m_tileDescriptorSet.IsValid())
		{
			UpdateLightBufferDescriptorSet();
		}
		else
		{
			m_shouldUpdateLightBuffer = true;
		}

		if (m_pPBRLightingStage.IsValid())
		{
			m_pPBRLightingStage->OnVisibleRenderItemTransformsChanged(renderItems);
		}
	}

	Threading::JobBatch TilePopulationStage::LoadRenderItemsResources(const Entity::RenderItemMask& renderItems)
	{
		if (m_pPBRLightingStage.IsValid())
		{
			return m_pPBRLightingStage->LoadRenderItemsResources(renderItems);
		}
		else
		{
			return {};
		}
	}

	void TilePopulationStage::OnSceneUnloaded()
	{
		for (Vector<ReferenceWrapper<const Entity::LightSourceComponent>>& lightContainer : m_visibleLights)
		{
			lightContainer.Clear();
		}
	}

	void TilePopulationStage::
		OnActiveCameraPropertiesChanged([[maybe_unused]] const Rendering::CommandEncoderView graphicsCommandEncoder, PerFrameStagingBuffer&)
	{
	}

	void TilePopulationStage::UpdateLightBuffer(
		const Rendering::CommandEncoderView graphicsCommandEncoder, PerFrameStagingBuffer& perFrameStagingBuffer

	)
	{
		{
			m_pointLights.Clear();
			for (const Entity::LightSourceComponent& lightComponent :
			     m_visibleLights[(uint8)LightTypes::PointLight].GetSubView(0, MaximumLightCounts[(uint8)LightTypes::PointLight]))
			{
				const Entity::PointLightComponent& pointLight = static_cast<const Entity::PointLightComponent&>(lightComponent);

				const Optional<const VisibleLight*> pLightInfo = m_lightGatheringStage.GetVisibleLightInfo(pointLight);

				const Math::WorldCoordinate location = pointLight.GetWorldLocation();
				const Math::Color color = pointLight.GetColorWithIntensity();

				const float radius = pointLight.GetInfluenceRadius().GetMeters();

				m_pointLights.EmplaceBack(PointLightInfo{
					{location.x, location.y, location.z, radius},
					{color.r, color.g, color.b, Math::MultiplicativeInverse(radius * radius)},
					pLightInfo.IsValid() ? pLightInfo->shadowMapIndex : -1,
					pLightInfo.IsValid() ? pLightInfo->shadowSampleViewProjectionMatrices.GetView() : ArrayView<const Math::Matrix4x4f, uint8>{},
				});
			}

			m_lightStorageBufferSizes[(uint8)LightTypes::PointLight] = m_pointLights.GetDataSize();

			if (m_pointLights.HasElements())
			{
				if (LightHeaderSizes[(uint8)LightTypes::PointLight] > 0)
				{
					LightHeader& header = m_lightHeaders[LightTypes::PointLight];
					header = {m_pointLights.GetSize()};

					perFrameStagingBuffer.CopyToBuffer(
						m_logicalDevice,
						m_logicalDevice.GetCommandQueue(QueueFamily::Graphics),
						graphicsCommandEncoder,
						ConstByteView::Make(header),
						m_lightStorageBuffers[(uint8)LightTypes::PointLight]
					);
				}

				perFrameStagingBuffer.CopyToBuffer(
					m_logicalDevice,
					m_logicalDevice.GetCommandQueue(QueueFamily::Graphics),
					graphicsCommandEncoder,
					ConstByteView(m_pointLights.GetView()),
					m_lightStorageBuffers[(uint8)LightTypes::PointLight],
					LightHeaderSizes[(uint8)LightTypes::PointLight]
				);
			};
		}

		{
			m_spotLights.Clear();

			for (const Entity::LightSourceComponent& lightComponent :
			     m_visibleLights[(uint8)LightTypes::SpotLight].GetSubView(0, MaximumLightCounts[(uint8)LightTypes::SpotLight]))
			{
				const Entity::SpotLightComponent& spotLight = static_cast<const Entity::SpotLightComponent&>(lightComponent);

				const Optional<const VisibleLight*> pLightInfo = m_lightGatheringStage.GetVisibleLightInfo(spotLight);

				const Math::WorldCoordinate location = spotLight.GetWorldLocation();
				const float radius = spotLight.GetInfluenceRadius().GetMeters();
				const Math::Color color = spotLight.GetColorWithIntensity();

				m_spotLights.EmplaceBack(SpotLightInfo{
					{location.x, location.y, location.z, radius},
					{color.r, color.g, color.b, Math::MultiplicativeInverse(radius * radius)},
					pLightInfo.IsValid() ? pLightInfo->shadowMapIndex : -1,
					pLightInfo.IsValid() ? pLightInfo->shadowSampleViewProjectionMatrices[0] : Math::Identity
				});
			}

			m_lightStorageBufferSizes[(uint8)LightTypes::SpotLight] = m_spotLights.GetDataSize();

			if (m_spotLights.HasElements())
			{
				if (LightHeaderSizes[(uint8)LightTypes::SpotLight] > 0)
				{
					LightHeader& header = m_lightHeaders[LightTypes::SpotLight];
					header = {m_spotLights.GetSize()};

					perFrameStagingBuffer.CopyToBuffer(
						m_logicalDevice,
						m_logicalDevice.GetCommandQueue(QueueFamily::Graphics),
						graphicsCommandEncoder,
						ConstByteView::Make(header),
						m_lightStorageBuffers[(uint8)LightTypes::SpotLight]
					);
				}

				perFrameStagingBuffer.CopyToBuffer(
					m_logicalDevice,
					m_logicalDevice.GetCommandQueue(QueueFamily::Graphics),
					graphicsCommandEncoder,
					ConstByteView(m_spotLights.GetView()),
					m_lightStorageBuffers[(uint8)LightTypes::SpotLight],
					LightHeaderSizes[(uint8)LightTypes::SpotLight]
				);
			};
		}

		{
			m_directionalLights.Clear();

			for (const Entity::LightSourceComponent& lightComponent :
			     m_visibleLights[(uint8)LightTypes::DirectionalLight].GetSubView(0, MaximumLightCounts[(uint8)LightTypes::DirectionalLight]))
			{
				const Entity::DirectionalLightComponent& directionalLight = static_cast<const Entity::DirectionalLightComponent&>(lightComponent);
				const Optional<const VisibleLight*> pLightInfo = m_lightGatheringStage.GetVisibleLightInfo(directionalLight);

				const Math::Vector3f direction = directionalLight.GetWorldForwardDirection();
				const Math::Color color = directionalLight.GetColorWithIntensity();

				m_directionalLights.EmplaceBack(
					DirectionalLightInfo {
#if !ENABLE_SAMPLE_DISTRIBUTION_SHADOW_MAPS
						pLightInfo.IsValid()
						? Math::
								Vector4f{pLightInfo->cascadeSplitDepths[0], pLightInfo->cascadeSplitDepths[1], pLightInfo->cascadeSplitDepths[2], pLightInfo->cascadeSplitDepths[3]}
						: Math::Vector4f(Math::Zero),
#endif
					{-direction.x, -direction.y, -direction.z},
					directionalLight.GetCascadeCount(),
					{color.r, color.g, color.b},
					pLightInfo.IsValid() ? pLightInfo->shadowMapIndex : -1,
#if ENABLE_SAMPLE_DISTRIBUTION_SHADOW_MAPS
					pLightInfo.IsValid() ? pLightInfo->directionalShadowMatrixIndex : 0
#else
							pLightInfo.IsValid() ? pLightInfo->shadowSampleViewProjectionMatrices.GetSubView(0, (uint8)directionalLight.GetCascadeCount())
																	 : ArrayView<const Math::Matrix4x4f, uint8>{},
#endif
					}
				);
			}

			m_lightStorageBufferSizes[(uint8)LightTypes::DirectionalLight] = m_directionalLights.GetDataSize();

			if (m_directionalLights.HasElements())
			{
				if (LightHeaderSizes[(uint8)LightTypes::DirectionalLight] > 0)
				{
					LightHeader& header = m_lightHeaders[LightTypes::DirectionalLight];
					header = {m_directionalLights.GetSize()};

					perFrameStagingBuffer.CopyToBuffer(
						m_logicalDevice,
						m_logicalDevice.GetCommandQueue(QueueFamily::Graphics),
						graphicsCommandEncoder,
						ConstByteView::Make(header),
						m_lightStorageBuffers[(uint8)LightTypes::DirectionalLight]
					);
				}

				perFrameStagingBuffer.CopyToBuffer(
					m_logicalDevice,
					m_logicalDevice.GetCommandQueue(QueueFamily::Graphics),
					graphicsCommandEncoder,
					ConstByteView(m_directionalLights.GetView()),
					m_lightStorageBuffers[(uint8)LightTypes::DirectionalLight],
					LightHeaderSizes[(uint8)LightTypes::DirectionalLight]
				);
			};
		}
	}

	void TilePopulationStage::UpdateLightBufferDescriptorSet()
	{
		Array<DescriptorSet::UpdateInfo, (uint8)LightTypes::RealLightCount> descriptorUpdates;
		Array<DescriptorSet::BufferInfo, (uint8)LightTypes::RealLightCount> bufferInfo;

		for (LightTypes lightType = LightTypes::First; lightType <= LightTypes::LastRealLight; lightType = LightTypes((uint8)lightType + 1))
		{
			const uint8 lightTypeIndex = (uint8)lightType;
			bufferInfo[lightTypeIndex] = DescriptorSet::BufferInfo{
				m_lightStorageBuffers[lightTypeIndex],
				0,
				LightHeaderSizes[lightTypeIndex] + Math::Max(m_lightStorageBufferSizes[lightTypeIndex], LightStructSizes[lightTypeIndex])
			};

			descriptorUpdates[lightTypeIndex] = DescriptorSet::UpdateInfo{
				m_tileDescriptorSet,
				DescriptorSetView::BindingIndexType(lightTypeIndex + 1),
				0,
				DescriptorType::StorageBuffer,
				ArrayView<const DescriptorSet::BufferInfo>(bufferInfo[lightTypeIndex])
			};
		}

		Threading::UniqueLock lock(m_descriptorMutex);
		DescriptorSet::Update(m_sceneView.GetLogicalDevice(), descriptorUpdates);
	}

	void TilePopulationStage::OnBeforeRenderPassDestroyed()
	{
		m_shouldUpdateLightBuffer = true;
	}

	bool TilePopulationStage::ShouldRecordCommands() const
	{
		return m_tileDescriptorSet.IsValid() & m_sceneView.HasActiveCamera() & RenderItemStage::ShouldRecordCommands() &
		       (!m_pPBRLightingStage.IsValid() || !m_pPBRLightingStage->EvaluateShouldSkip()) & m_tilePopulationPipeline.IsValid();
	}

	void TilePopulationStage::OnBeforeRecordCommands(const CommandEncoderView)
	{
		if (WasSkipped())
		{
			return;
		}

		bool expected = true;
		if (m_tileDescriptorSet.IsValid() && m_shouldUpdateLightBuffer.CompareExchangeStrong(expected, false))
		{
			UpdateLightBufferDescriptorSet();
		}
	}

	void TilePopulationStage::RecordComputePassCommands(
		const ComputeCommandEncoderView computeCommandEncoder, const ViewMatrices& viewMatrices, [[maybe_unused]] const uint8 subpassIndex
	)
	{
		computeCommandEncoder.BindPipeline(m_tilePopulationPipeline);
		const Math::Vector2ui renderResolution = (Math::Vector2ui)(Math::Vector2f(viewMatrices.GetRenderResolution()) * UpscalingFactor);
		const Math::Vector2ui tileSize = CalculateTileSize(renderResolution);
		m_tilePopulationPipeline.Compute(
			m_logicalDevice,
			Array<const DescriptorSetView, 2>(m_sceneView.GetMatrices().GetDescriptorSet(), m_tileDescriptorSet.AtomicLoad()),
			computeCommandEncoder,
			Math::Vector4ui{
				Math::Min(m_visibleLights[(uint8)LightTypes::PointLight].GetSize(), (uint32)MaximumLightCounts[(uint8)LightTypes::PointLight]),
				Math::Min(m_visibleLights[(uint8)LightTypes::SpotLight].GetSize(), (uint32)MaximumLightCounts[(uint8)LightTypes::SpotLight]),
				Math::Min(
					m_visibleLights[(uint8)LightTypes::DirectionalLight].GetSize(),
					(uint32)MaximumLightCounts[(uint8)LightTypes::DirectionalLight]
				),
				Math::Min(
					m_visibleLights[(uint8)LightTypes::EnvironmentLight].GetSize(),
					(uint32)MaximumLightCounts[(uint8)LightTypes::EnvironmentLight]
				),
			},
			tileSize,
			m_passIndex == 0 ? Math::Zero : Math::Vector2ui{tileSize.x, 0}
		);
	}
}
