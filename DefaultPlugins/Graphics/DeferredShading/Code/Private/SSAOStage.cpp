#include "SSAOStage.h"
#include "PBRLightingStage.h"

#include <Common/Threading/Jobs/AsyncJob.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/Threading/Jobs/JobBatch.h>
#include <Common/Math/LinearInterpolate.h>
#include <Common/Math/Vector3/Random.h>
#include <Common/Math/PseudoRandomDistributions.h>
#include <Common/Memory/AddressOf.h>

#include <Engine/Threading/JobRunnerThread.h>

#include <Common/System/Query.h>

#include <Renderer/Commands/CommandEncoderView.h>
#include <Renderer/Commands/RenderCommandEncoder.h>
#include <Renderer/Commands/BlitCommandEncoder.h>
#include <Renderer/Commands/SingleUseCommandBuffer.h>
#include <Renderer/Commands/ClearValue.h>
#include <Renderer/Scene/SceneView.h>
#include <Renderer/Renderer.h>
#include <Renderer/Stages/StartFrameStage.h>
#include <Renderer/Wrappers/AttachmentDescription.h>
#include <Renderer/Wrappers/AttachmentReference.h>
#include <Renderer/Wrappers/SubpassDependency.h>
#include <Renderer/RenderOutput/RenderOutput.h>
#include <Renderer/Assets/Texture/RenderTexture.h>
#include <Renderer/Assets/Texture/MipMask.h>
#include <Renderer/Stages/RenderItemStage.h>
#include <Renderer/FormatInfo.h>
#include <Renderer/Buffers/StagingBuffer.h>
#include <Renderer/Buffers/DataToBufferBatch.h>
#include <DeferredShading/FSR/fsr_settings.h>

namespace ngine::Rendering
{
	SSAOStage::SSAOStage(SceneView& sceneView)
		: Stage(sceneView.GetLogicalDevice(), Threading::JobPriority::Draw)
		, m_sceneView(sceneView)
		, m_pipeline(
				m_sceneView.GetLogicalDevice(), m_sceneView.GetLogicalDevice().GetShaderCache(), m_sceneView.GetMatrices().GetDescriptorSetLayout()
			)
		// , m_blurSimplePipeline(m_sceneView.GetLogicalDevice(), m_sceneView.GetLogicalDevice().GetShaderCache())
		, m_normalsSampler(m_sceneView.GetLogicalDevice(), AddressMode::ClampToEdge, FilterMode::Linear)
		// , m_bilinearSampler(m_sceneView.GetLogicalDevice(), AddressMode::Repeat, FilterMode::Linear)
		, m_kernelStorageBuffer{
				m_sceneView.GetLogicalDevice(),
				m_sceneView.GetLogicalDevice().GetPhysicalDevice(),
				m_sceneView.GetLogicalDevice().GetDeviceMemoryPool(),
				KernelBufferSize,
			}
	{
		InitSSAOKernel();
	}

	SSAOStage::~SSAOStage()
	{
		Rendering::LogicalDevice& logicalDevice = m_sceneView.GetLogicalDevice();

		m_pipeline.Destroy(logicalDevice);
		// m_blurSimplePipeline.Destroy(logicalDevice);

		if (m_pDescriptorSetLoadingThread != nullptr)
		{
			Threading::EngineJobRunnerThread& previousEngineThread = static_cast<Threading::EngineJobRunnerThread&>(*m_pDescriptorSetLoadingThread
			);
			previousEngineThread.GetRenderData().DestroyDescriptorSets(logicalDevice.GetIdentifier(), m_descriptorSets.GetView());
		}

		m_normalsSampler.Destroy(logicalDevice);
		// m_bilinearSampler.Destroy(logicalDevice);

		m_kernelStorageBuffer.Destroy(logicalDevice, logicalDevice.GetDeviceMemoryPool());

		m_noiseImageMapping.Destroy(logicalDevice);

		TextureCache& textureCache = m_sceneView.GetLogicalDevice().GetRenderer().GetTextureCache();
		const TextureIdentifier textureIdentifier = textureCache.FindOrRegisterAsset(NoiseTextureAssetGuid);
		textureCache.RemoveRenderTextureListener(logicalDevice.GetIdentifier(), textureIdentifier, this);
	}

	Threading::JobBatch SSAOStage::LoadFixedResources()
	{
		// Init render targets and textures

		m_numLoadingTextures = (uint8)Images::Count;

		Threading::JobBatch batch;

		TextureCache& textureCache = m_sceneView.GetLogicalDevice().GetRenderer().GetTextureCache();

		const TextureIdentifier textureIdentifier = textureCache.FindOrRegisterAsset(NoiseTextureAssetGuid);

		Threading::Job* pTextureJob = textureCache.GetOrLoadRenderTexture(
			m_sceneView.GetLogicalDevice().GetIdentifier(),
			textureIdentifier,
			ImageMappingType::TwoDimensional,
			MipMask::FromIndex(4),
			TextureLoadFlags{},
			TextureCache::TextureLoadListenerData(
				*this,
				[this](SSAOStage&, const LogicalDevice& logicalDevice, const TextureIdentifier, RenderTexture& texture, const MipMask, const EnumFlags<LoadedTextureFlags>)
					-> EventCallbackResult
				{
					Threading::UniqueLock lock(m_textureLoadMutex);
					ImageMapping newMapping(
						logicalDevice,
						texture,
						ImageMappingType::TwoDimensional,
						texture.GetFormat(),
						ImageAspectFlags::Color,
						texture.GetAvailableMipRange(),
						ArrayRange{0, texture.GetTotalArrayCount()}
					);

					m_imageMappings[Images::Noise] = newMapping;
					m_noiseImageMapping.AtomicSwap(newMapping);
					if (!newMapping.IsValid())
					{
						// Only populate the descriptor set when all textures are accounted for
						if (--m_numLoadingTextures == 0)
						{
							OnFinishedLoadingTextures();
						}
					}
					else if (m_numLoadingTextures > 0)
					{
						Threading::EngineJobRunnerThread& thread =
							static_cast<Threading::EngineJobRunnerThread&>(*Threading::JobRunnerThread::GetCurrent());
						thread.GetRenderData().DestroyImageMapping(logicalDevice.GetIdentifier(), Move(newMapping));
					}
					else
					{
						Threading::JobRunnerThread* pPreviousDescriptorLoadingThread = m_pDescriptorSetLoadingThread;

						Threading::EngineJobRunnerThread& thread =
							static_cast<Threading::EngineJobRunnerThread&>(*Threading::JobRunnerThread::GetCurrent());

						Array<DescriptorSetLayoutView, (uint8)Stages::Count, Stages> descriptorSetLayouts{
							m_pipeline,
							// m_blurSimplePipeline,
						};

						Array<DescriptorSet, (uint8)Stages::Count, Stages> descriptorSets;
						const DescriptorPoolView descriptorPool = thread.GetRenderData().GetDescriptorPool(m_sceneView.GetLogicalDevice().GetIdentifier(
						));
						[[maybe_unused]] const bool allocatedDescriptorSets = descriptorPool.AllocateDescriptorSets(
							m_sceneView.GetLogicalDevice(),
							descriptorSetLayouts.GetDynamicView(),
							descriptorSets.GetDynamicView()
						);
						Assert(allocatedDescriptorSets);
						if (LIKELY(allocatedDescriptorSets))
						{
							Assert(allocatedDescriptorSets);
							m_pDescriptorSetLoadingThread = &thread;

							PopulateDescriptorSets(descriptorSets.GetView());

							for (uint8 i = 0; i < (uint8)Stages::Count; ++i)
							{
								m_descriptorSets[(Stages)i].AtomicSwap(descriptorSets[(Stages)i]);
							}

							Assert(pPreviousDescriptorLoadingThread != nullptr);
							if (LIKELY(pPreviousDescriptorLoadingThread != nullptr))
							{
								Threading::EngineJobRunnerThread& previousLoadingThread =
									static_cast<Threading::EngineJobRunnerThread&>(*pPreviousDescriptorLoadingThread);
								previousLoadingThread.GetRenderData().DestroyDescriptorSets(logicalDevice.GetIdentifier(), descriptorSets.GetView());
							}
						}

						thread.GetRenderData().DestroyImageMapping(logicalDevice.GetIdentifier(), Move(newMapping));
					}

					return EventCallbackResult::Remove;
				}
			)
		);
		if (pTextureJob != nullptr)
		{
			batch.QueueAfterStartStage(*pTextureJob);
		}

		return batch;
	}

	void SSAOStage::OnFinishedLoadingTextures()
	{
		Threading::JobRunnerThread* pPreviousDescriptorLoadingThread = m_pDescriptorSetLoadingThread;

		LogicalDevice& logicalDevice = m_sceneView.GetLogicalDevice();

		Threading::EngineJobRunnerThread& thread = static_cast<Threading::EngineJobRunnerThread&>(*Threading::JobRunnerThread::GetCurrent());
		Array<DescriptorSetLayoutView, (uint8)Stages::Count, Stages> descriptorSetLayouts{
			m_pipeline,
			// m_blurSimplePipeline,
		};

		Array<DescriptorSet, (uint8)Stages::Count, Stages> descriptorSets;
		const DescriptorPoolView descriptorPool = thread.GetRenderData().GetDescriptorPool(logicalDevice.GetIdentifier());
		[[maybe_unused]] const bool allocatedDescriptorSets =
			descriptorPool.AllocateDescriptorSets(logicalDevice, descriptorSetLayouts.GetDynamicView(), descriptorSets.GetDynamicView());
		Assert(allocatedDescriptorSets);
		if (LIKELY(allocatedDescriptorSets))
		{
			Assert(allocatedDescriptorSets);
			m_pDescriptorSetLoadingThread = &thread;

			PopulateDescriptorSets(descriptorSets.GetView());

			for (uint8 i = 0; i < (uint8)Stages::Count; ++i)
			{
				m_descriptorSets[(Stages)i].AtomicSwap(descriptorSets[(Stages)i]);
			}

			if (pPreviousDescriptorLoadingThread != nullptr)
			{
				Threading::EngineJobRunnerThread& previousLoadingThread =
					static_cast<Threading::EngineJobRunnerThread&>(*pPreviousDescriptorLoadingThread);
				previousLoadingThread.GetRenderData().DestroyDescriptorSets(logicalDevice.GetIdentifier(), descriptorSets.GetView());
			}
		}

		m_loadedAllTextures = true;
	}

	void SSAOStage::InitSSAOKernel()
	{
		Math::UniformHemisphereDistributionUsingGoldenSpiral(KernelBuffer::KernelSize, m_kernelBuffer.samples);
		for (uint32_t i = 0; i < KernelBuffer::KernelSize; ++i)
		{
			Math::Vector3f sample = m_kernelBuffer.samples[i];

			sample *= Math::Random(0.f, 1.f);
			float scale = float(i) / float(KernelBuffer::KernelSize);
			scale = Math::LinearInterpolate(0.1f, 1.0f, scale * scale);
			m_kernelBuffer.samples[i] = sample * scale;
		}

		LogicalDevice& logicalDevice = m_sceneView.GetLogicalDevice();

		Threading::EngineJobRunnerThread& thread = *Threading::EngineJobRunnerThread::GetCurrent();
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
				DataToBufferBatch{m_kernelStorageBuffer, Array<const DataToBuffer, 1>{DataToBuffer{0, ConstByteView::Make(m_kernelBuffer)}}}
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

	void SSAOStage::PopulateDescriptorSets(const FixedArrayView<Rendering::DescriptorSet, (uint8)Stages::Count, Stages> descriptorSets)
	{
		const Array ssaoImageInfos{
			DescriptorSet::ImageInfo{{}, m_imageMappings[Images::Depth], ImageLayout::DepthStencilReadOnlyOptimal},
			DescriptorSet::ImageInfo{{}, m_imageMappings[Images::Normals], ImageLayout::ShaderReadOnlyOptimal},
			DescriptorSet::ImageInfo{m_normalsSampler, {}, ImageLayout::ShaderReadOnlyOptimal},
			DescriptorSet::ImageInfo{{}, m_imageMappings[Images::Noise], ImageLayout::ShaderReadOnlyOptimal},
			// DescriptorSet::ImageInfo{{}, m_imageMappings[Images::SSAO], ImageLayout::General}
			DescriptorSet::ImageInfo{{}, m_imageMappings[Images::HDR], ImageLayout::General}
		};
		const Array ssaoBufferInfo{DescriptorSet::BufferInfo{m_kernelStorageBuffer, 0, KernelBufferSize}};

		// const Array blurSimpleImageInfo{
		// 	DescriptorSet::ImageInfo{{}, m_imageMappings[Images::SSAO], ImageLayout::ShaderReadOnlyOptimal},
		// 	DescriptorSet::ImageInfo{{}, m_imageMappings[Images::SSAOBlur], ImageLayout::General},
		// 	DescriptorSet::ImageInfo{m_bilinearSampler, {}, ImageLayout::Undefined}
		// };

		const Array descriptorUpdates{
			DescriptorSet::UpdateInfo{descriptorSets[Stages::SSAO], 0, 0, DescriptorType::SampledImage, ssaoImageInfos.GetSubView(0, 1)},
			DescriptorSet::UpdateInfo{descriptorSets[Stages::SSAO], 1, 0, DescriptorType::SampledImage, ssaoImageInfos.GetSubView(1, 1)},
			DescriptorSet::UpdateInfo{descriptorSets[Stages::SSAO], 2, 0, DescriptorType::Sampler, ssaoImageInfos.GetSubView(2, 1)},
			DescriptorSet::UpdateInfo{descriptorSets[Stages::SSAO], 3, 0, DescriptorType::SampledImage, ssaoImageInfos.GetSubView(3, 1)},
			DescriptorSet::UpdateInfo{descriptorSets[Stages::SSAO], 4, 0, DescriptorType::StorageImage, ssaoImageInfos.GetSubView(4, 1)},
			DescriptorSet::UpdateInfo{descriptorSets[Stages::SSAO], 5, 0, DescriptorType::StorageBuffer, ssaoBufferInfo.GetSubView(0, 1)},
			// DescriptorSet::UpdateInfo{
		  // 	descriptorSets[Stages::BlurSimple],
		  // 	0,
		  // 	0,
		  // 	DescriptorType::SampledImage,
		  // 	blurSimpleImageInfo.GetSubView(0, 1)
		  // },
		  // DescriptorSet::UpdateInfo{
		  // 	descriptorSets[Stages::BlurSimple],
		  // 	1,
		  // 	0,
		  // 	DescriptorType::StorageImage,
		  // 	blurSimpleImageInfo.GetSubView(1, 1)
		  // },
		  // DescriptorSet::UpdateInfo{descriptorSets[Stages::BlurSimple], 2, 0, DescriptorType::Sampler, blurSimpleImageInfo.GetSubView(2, 1)},
		};
		DescriptorSet::Update(m_sceneView.GetLogicalDevice(), descriptorUpdates);
	}

	void SSAOStage::OnBeforeRenderPassDestroyed()
	{
		m_loadedAllTextures = false;
		m_numLoadingTextures = (uint8)Images::Count - 1;

		// m_imageMappings[Images::SSAO] = {};
		// m_imageMappings[Images::SSAOBlur] = {};
		m_imageMappings[Images::HDR] = {};
		m_imageMappings[Images::Depth] = {};
		m_imageMappings[Images::Normals] = {};
	}

	bool SSAOStage::ShouldRecordCommands() const
	{
		return m_loadedAllTextures & m_sceneView.HasActiveCamera() & m_pipeline.IsValid(); // & m_blurSimplePipeline.IsValid();
	}

	void SSAOStage::RecordComputePassCommands(
		const ComputeCommandEncoderView computeCommandEncoder, const ViewMatrices& viewMatrices, [[maybe_unused]] const uint8 subpassIndex
	)
	{
		const Math::Vector2ui renderResolution = (Math::Vector2ui)(Math::Vector2f(viewMatrices.GetRenderResolution()) * UpscalingFactor);
		switch ((Stages)subpassIndex)
		{
			case Stages::SSAO:
			{
				computeCommandEncoder.BindPipeline(m_pipeline);
				m_pipeline.Compute(
					Array<const DescriptorSetView, 2>(m_sceneView.GetMatrices().GetDescriptorSet(), m_descriptorSets[Stages::SSAO].AtomicLoad()),
					computeCommandEncoder,
					renderResolution
				);
			}
			break;
			// case Stages::BlurSimple:
			// {
			// 	computeCommandEncoder.BindPipeline(m_blurSimplePipeline);
			// 	m_blurSimplePipeline.Compute(
			// 		ArrayView<const DescriptorSetView>(m_descriptorSets[Stages::BlurSimple].AtomicLoad()),
			// 		computeCommandEncoder,
			// 		renderResolution,
			// 		Math::Vector2i(1, 0)
			// 	);
			// }
			// break;
			case Stages::Count:
				ExpectUnreachable();
		}
	}

	void SSAOStage::OnComputePassAttachmentsLoaded(
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
		switch ((Stages)subpassIndex)
		{
			case Stages::SSAO:
			{
				m_imageMappings[Images::HDR] = outputInputAttachmentMappings[0][0];
				// m_imageMappings[Images::SSAO] = outputAttachmentMappings[0][0];
				m_imageMappings[Images::Depth] = inputAttachmentMappings[0][0];
				m_imageMappings[Images::Normals] = inputAttachmentMappings[0][1];
				if (m_numLoadingTextures.FetchSubtract(3) == 3)
				{
					OnFinishedLoadingTextures();
				}
			}
			break;
			// case Stages::BlurSimple:
			// {
			// 	m_imageMappings[Images::SSAOBlur] = outputAttachmentMappings[0][0];
			// 	if (m_numLoadingTextures.FetchSubtract(1) == 1)
			// 	{
			// 		OnFinishedLoadingTextures();
			// 	}
			// }
			// break;
			case Stages::Count:
				ExpectUnreachable();
		}
	}
}
