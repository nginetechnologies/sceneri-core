#include "PostProcessStage.h"

#include <Common/Threading/Jobs/AsyncJob.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/Threading/Jobs/JobBatch.h>
#include <Common/Math/LinearInterpolate.h>
#include <Common/Math/Vector3/Random.h>
#include <Common/Memory/AddressOf.h>

#include <Engine/Threading/JobRunnerThread.h>

#include <Common/System/Query.h>
#include <Engine/Entity/CameraComponent.h>

#include <Renderer/Commands/CommandEncoderView.h>
#include <Renderer/Commands/ComputeCommandEncoder.h>
#include <Renderer/Commands/BlitCommandEncoder.h>
#include <Renderer/Commands/BarrierCommandEncoder.h>
#include <Renderer/Commands/SingleUseCommandBuffer.h>
#include <Renderer/Scene/SceneView.h>
#include <Renderer/Scene/SceneViewDrawer.h>
#include <Renderer/Renderer.h>
#include <Renderer/Stages/StartFrameStage.h>
#include <Renderer/Wrappers/AttachmentDescription.h>
#include <Renderer/Wrappers/AttachmentReference.h>
#include <Renderer/Wrappers/SubpassDependency.h>
#include <Renderer/RenderOutput/RenderOutput.h>
#include <Renderer/Assets/Texture/RenderTexture.h>
#include <Renderer/Assets/Texture/MipMask.h>
#include <Renderer/Stages/RenderItemStage.h>
#include <Renderer/Buffers/StagingBuffer.h>
#include <Renderer/Buffers/DataToBufferBatch.h>

namespace ngine::Rendering
{
	PostProcessStage::PostProcessStage(SceneView& sceneView)
		: Stage(sceneView.GetLogicalDevice(), Threading::JobPriority::Draw)
		, m_sceneView(sceneView)
		, m_downsamplePipeline(m_sceneView.GetLogicalDevice(), m_sceneView.GetLogicalDevice().GetShaderCache())
		, m_blurPipeline(m_sceneView.GetLogicalDevice(), m_sceneView.GetLogicalDevice().GetShaderCache())
		, m_lensFlarePipeline(m_sceneView.GetLogicalDevice(), m_sceneView.GetLogicalDevice().GetShaderCache())
		, m_compositePostProcessPipeline(m_sceneView.GetLogicalDevice(), m_sceneView.GetLogicalDevice().GetShaderCache())
		, m_temporalAntiAliasingResolvePipeline(m_sceneView.GetLogicalDevice(), m_sceneView.GetLogicalDevice().GetShaderCache())
		, m_superResolutionPipeline(m_sceneView.GetLogicalDevice(), m_sceneView.GetLogicalDevice().GetShaderCache())
		, m_sharpenPipeline(m_sceneView.GetLogicalDevice(), m_sceneView.GetLogicalDevice().GetShaderCache())
		, m_bilinearSampler(m_sceneView.GetLogicalDevice(), AddressMode::Repeat, FilterMode::Linear)
		, m_fsrSampler(
				m_sceneView.GetLogicalDevice(),
				AddressMode::ClampToEdge,
				FilterMode::Linear,
				MipmapMode::Nearest,
				CompareOperation::AlwaysSucceed,
				Math::Range<int16>::MakeStartToEnd(0, 1000)
			) // From FSR_Filter.cpp example
		, m_temporalAntiAliasingCompositeSampler(m_sceneView.GetLogicalDevice(), AddressMode::ClampToEdge, FilterMode::Nearest)
		, m_temporalAntiAliasingHistorySampler(m_sceneView.GetLogicalDevice(), AddressMode::ClampToEdge, FilterMode::Linear)
		, m_gaussianWeightBuffer{
				m_sceneView.GetLogicalDevice(),
				m_sceneView.GetLogicalDevice().GetPhysicalDevice(),
				m_sceneView.GetLogicalDevice().GetDeviceMemoryPool(),
				sizeof(float) * (GaussianBlurKernelSize + 1),
			}
	{
		InitGaussianBlurKernel();
	}

	PostProcessStage::~PostProcessStage()
	{
		LogicalDevice& logicalDevice = m_sceneView.GetLogicalDevice();

		m_downsamplePipeline.Destroy(logicalDevice);
		m_blurPipeline.Destroy(logicalDevice);
		m_lensFlarePipeline.Destroy(logicalDevice);
		m_compositePostProcessPipeline.Destroy(logicalDevice);
		m_temporalAntiAliasingResolvePipeline.Destroy(logicalDevice);
		m_superResolutionPipeline.Destroy(logicalDevice);
		m_sharpenPipeline.Destroy(logicalDevice);

		if (m_pDescriptorSetLoadingThread != nullptr)
		{
			Threading::EngineJobRunnerThread& previousEngineThread = static_cast<Threading::EngineJobRunnerThread&>(*m_pDescriptorSetLoadingThread
			);
			previousEngineThread.GetRenderData().DestroyDescriptorSets(logicalDevice.GetIdentifier(), m_descriptorSets.GetView());
		}

		m_bilinearSampler.Destroy(logicalDevice);

		m_fsrSampler.Destroy(logicalDevice);

		m_temporalAntiAliasingCompositeSampler.Destroy(logicalDevice);
		m_temporalAntiAliasingHistorySampler.Destroy(logicalDevice);

		m_gaussianWeightBuffer.Destroy(logicalDevice, logicalDevice.GetDeviceMemoryPool());

		m_lensDirtImageMapping.Destroy(logicalDevice);

		TextureCache& textureCache = m_sceneView.GetLogicalDevice().GetRenderer().GetTextureCache();
		textureCache.RemoveRenderTextureListener(
			m_sceneView.GetLogicalDevice().GetIdentifier(),
			textureCache.FindOrRegisterAsset(LensDirtTextureAssetGuid),
			this
		);
	}

	Threading::JobBatch PostProcessStage::LoadFixedResources()
	{
		// Init render targets and textures

		m_loadingTextureMask |= ImagesMask(Images::RequiredMask);

		Threading::JobBatch batch;

		TextureCache& textureCache = m_sceneView.GetLogicalDevice().GetRenderer().GetTextureCache();

		const TextureIdentifier textureIdentifier = textureCache.FindOrRegisterAsset(LensDirtTextureAssetGuid);

		Threading::Job* pTextureJob = textureCache.GetOrLoadRenderTexture(
			m_sceneView.GetLogicalDevice().GetIdentifier(),
			textureIdentifier,
			ImageMappingType::TwoDimensional,
			AllMips,
			TextureLoadFlags{},
			TextureCache::TextureLoadListenerData(
				*this,
				[this,
		     textureIndex = (ImagesMask)Images::
		       LensDirt](PostProcessStage&, const LogicalDevice& logicalDevice, const TextureIdentifier, RenderTexture& texture, const MipMask, const EnumFlags<LoadedTextureFlags>)
					-> EventCallbackResult
				{
					OnTextureLoaded(textureIndex, logicalDevice, texture);
					if (texture.HasLoadedAllMips())
					{
						return EventCallbackResult::Remove;
					}
					else
					{
						return EventCallbackResult::Keep;
					}
				}
			)
		);
		if (pTextureJob != nullptr)
		{
			batch.QueueAfterStartStage(*pTextureJob);
		}

		return batch;
	}

	void PostProcessStage::OnTextureLoaded(const ImagesMask textureIndex, const LogicalDevice& logicalDevice, RenderTexture& texture)
	{
		Threading::UniqueLock lock(m_descriptorMutex);
		ImageMapping newMapping(
			logicalDevice,
			texture,
			ImageMappingType::TwoDimensional,
			texture.GetFormat(),
			ImageAspectFlags::Color,
			texture.GetAvailableMipRange(),
			ArrayRange{0, texture.GetTotalArrayCount()}
		);

		m_imageMappings[(Images)textureIndex] = newMapping;
		m_lensDirtImageMapping.AtomicSwap(newMapping);

		// Only populate the descriptor set when all textures are accounted for
		const ImagesMask removedMask = ImagesMask(1 << textureIndex);
		const ImagesMask previousMask = m_loadingTextureMask.FetchAnd((ImagesMask)~removedMask);
		if ((previousMask & removedMask) != 0 && (previousMask & ~removedMask) == 0)
		{
			lock.Unlock();
			OnFinishedLoadingTextures();
		}
		else if (previousMask != 0)
		{
			if (newMapping.IsValid())
			{
				// We were still loading textures, send the old mip away immediately
				Threading::EngineJobRunnerThread& thread = static_cast<Threading::EngineJobRunnerThread&>(*Threading::JobRunnerThread::GetCurrent()
				);
				thread.GetRenderData().DestroyImageMapping(logicalDevice.GetIdentifier(), Move(newMapping));
			}
		}
		else
		{
			// Update the descriptor set
			Threading::JobRunnerThread* pPreviousDescriptorLoadingThread = m_pDescriptorSetLoadingThread;

			Threading::EngineJobRunnerThread& thread = static_cast<Threading::EngineJobRunnerThread&>(*Threading::JobRunnerThread::GetCurrent());

			Array<DescriptorSetLayoutView, (uint8)Stages::Count, Stages> descriptorSetLayouts{
				m_downsamplePipeline,
				m_lensFlarePipeline,
				m_blurPipeline,
				m_blurPipeline,
				m_compositePostProcessPipeline,
				m_temporalAntiAliasingResolvePipeline,
				m_superResolutionPipeline,
				m_sharpenPipeline,
			};

			Array<DescriptorSet, (uint8)Stages::Count, Stages> descriptorSets;
			const DescriptorPoolView descriptorPool = thread.GetRenderData().GetDescriptorPool(m_sceneView.GetLogicalDevice().GetIdentifier());
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

			// Create the old mapping swapped into newMapping
			thread.GetRenderData().DestroyImageMapping(logicalDevice.GetIdentifier(), Move(newMapping));
		}
	}

	void PostProcessStage::OnFinishedLoadingTextures()
	{
		Threading::UniqueLock lock(m_descriptorMutex);
		Threading::JobRunnerThread* pPreviousDescriptorLoadingThread = m_pDescriptorSetLoadingThread;

		LogicalDevice& logicalDevice = m_sceneView.GetLogicalDevice();

		Threading::EngineJobRunnerThread& thread = static_cast<Threading::EngineJobRunnerThread&>(*Threading::JobRunnerThread::GetCurrent());
		Array<DescriptorSetLayoutView, (uint8)Stages::Count, Stages> descriptorSetLayouts{
			m_downsamplePipeline,
			m_lensFlarePipeline,
			m_blurPipeline,
			m_blurPipeline,
			m_compositePostProcessPipeline,
			m_temporalAntiAliasingResolvePipeline,
			m_superResolutionPipeline,
			m_sharpenPipeline,
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

	void PostProcessStage::InitGaussianBlurKernel()
	{
		float totalWeight = 0.0f;

		const float smallestWeight = 0.2f;
		float scale = -(GaussianBlurKernelSize * GaussianBlurKernelSize) / logf(smallestWeight);

		m_gaussianBlurKernel.GetView().ZeroInitialize();

		for (uint8 i = 0; i <= GaussianBlurKernelSize; ++i)
		{
			float w = expf(-float(i * i) / scale);

			totalWeight += w;
			m_gaussianBlurKernel[i] = w;
		}

		// Normalize the kernel
		for (uint8 i = 0; i <= GaussianBlurKernelSize; ++i)
		{
			m_gaussianBlurKernel[i] /= totalWeight;
		}
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
			Array<const DataToBufferBatch, 1>{
				DataToBufferBatch{m_gaussianWeightBuffer, Array<const DataToBuffer, 1>{DataToBuffer{0, ConstByteView::Make(m_gaussianBlurKernel)}}}
			},
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

	void PostProcessStage::PopulateDescriptorSets(const FixedArrayView<Rendering::DescriptorSet, (uint8)Stages::Count, Stages> descriptorSets)
	{
		const Array downsampleImageInfo{
			DescriptorSet::ImageInfo{{}, m_imageMappings[Images::HDR], ImageLayout::ShaderReadOnlyOptimal},
			DescriptorSet::ImageInfo{{}, m_imageMappings[Images::Downsampled], ImageLayout::General},
		};
		const Array lensFlareGenerationImageInfo{
			DescriptorSet::ImageInfo{{}, m_imageMappings[Images::Downsampled], ImageLayout::ShaderReadOnlyOptimal},
			DescriptorSet::ImageInfo{{}, m_imageMappings[Images::LensFlare], ImageLayout::General},
			DescriptorSet::ImageInfo{m_bilinearSampler, {}, ImageLayout::Undefined},
		};
		const Array horizontalBlurImageInfo{
			DescriptorSet::ImageInfo{{}, m_imageMappings[Images::LensFlare], ImageLayout::ShaderReadOnlyOptimal},
			DescriptorSet::ImageInfo{{}, m_imageMappings[Images::Downsampled], ImageLayout::General},
		};
		const Array verticalBlurImageInfos{
			DescriptorSet::ImageInfo{{}, m_imageMappings[Images::Downsampled], ImageLayout::ShaderReadOnlyOptimal},
			DescriptorSet::ImageInfo{{}, m_imageMappings[Images::LensFlare], ImageLayout::General},
		};
		const Array blurBufferInfos{
			DescriptorSet::BufferInfo{m_gaussianWeightBuffer, 0, sizeof(float) * (GaussianBlurKernelSize + 1)},
		};
		const Array compositeImageInfos{
			DescriptorSet::ImageInfo{{}, m_imageMappings[Images::HDR], ImageLayout::ShaderReadOnlyOptimal},
			DescriptorSet::ImageInfo{{}, m_imageMappings[Images::LensFlare], ImageLayout::ShaderReadOnlyOptimal},
			DescriptorSet::ImageInfo{{}, m_imageMappings[Images::LensDirt], ImageLayout::ShaderReadOnlyOptimal},
			DescriptorSet::ImageInfo{{}, m_imageMappings[Images::Composite], ImageLayout::General},
			DescriptorSet::ImageInfo{m_bilinearSampler, {}, ImageLayout::Undefined},
		};
#if ENABLE_TAA
		const Array temporalAAResolveImageInfos{
			DescriptorSet::ImageInfo{{}, m_imageMappings[Images::Composite], ImageLayout::ShaderReadOnlyOptimal},
			DescriptorSet::ImageInfo{{}, m_imageMappings[Images::TemporalAAHistory], ImageLayout::ShaderReadOnlyOptimal},
			DescriptorSet::ImageInfo{{}, m_imageMappings[Images::HDR], ImageLayout::General},
			DescriptorSet::ImageInfo{m_temporalAntiAliasingCompositeSampler, {}, ImageLayout::Undefined},
			DescriptorSet::ImageInfo{m_temporalAntiAliasingHistorySampler, {}, ImageLayout::Undefined},
			DescriptorSet::ImageInfo{{}, m_imageMappings[Images::TemporalAAVelocity], ImageLayout::ShaderReadOnlyOptimal},
		};
#endif
#if ENABLE_FSR
		const Array superResolutionImageInfos{
			DescriptorSet::ImageInfo{{}, m_imageMappings[Images::Composite], ImageLayout::ShaderReadOnlyOptimal},
			DescriptorSet::ImageInfo{{}, m_imageMappings[Images::SuperResolution], ImageLayout::General},
			DescriptorSet::ImageInfo{m_fsrSampler, {}, ImageLayout::Undefined}
		};
#endif
		const Array descriptorUpdates
		{
			DescriptorSet::UpdateInfo{
				descriptorSets[Stages::Downsample],
				0,
				0,
				DescriptorType::SampledImage,
				downsampleImageInfo.GetSubView(0, 1)
			},
				DescriptorSet::UpdateInfo{
					descriptorSets[Stages::Downsample],
					1,
					0,
					DescriptorType::StorageImage,
					downsampleImageInfo.GetSubView(1, 1)
				},
				DescriptorSet::UpdateInfo{
					descriptorSets[Stages::LensflareGeneration],
					0,
					0,
					DescriptorType::SampledImage,
					lensFlareGenerationImageInfo.GetSubView(0, 1)
				},
				DescriptorSet::UpdateInfo{
					descriptorSets[Stages::LensflareGeneration],
					1,
					0,
					DescriptorType::StorageImage,
					lensFlareGenerationImageInfo.GetSubView(1, 1)
				},
				DescriptorSet::UpdateInfo{
					descriptorSets[Stages::LensflareGeneration],
					2,
					0,
					DescriptorType::Sampler,
					lensFlareGenerationImageInfo.GetSubView(2, 1)
				},
				DescriptorSet::UpdateInfo{
					descriptorSets[Stages::LensflareHorizontalBlur],
					0,
					0,
					DescriptorType::SampledImage,
					horizontalBlurImageInfo.GetSubView(0, 1)
				},
				DescriptorSet::UpdateInfo{
					descriptorSets[Stages::LensflareHorizontalBlur],
					1,
					0,
					DescriptorType::StorageImage,
					horizontalBlurImageInfo.GetSubView(1, 1)
				},
				DescriptorSet::UpdateInfo{
					descriptorSets[Stages::LensflareHorizontalBlur],
					2,
					0,
					DescriptorType::StorageBuffer,
					blurBufferInfos.GetSubView(0, 1)
				},
				DescriptorSet::UpdateInfo{
					descriptorSets[Stages::LensflareVerticalBlur],
					0,
					0,
					DescriptorType::SampledImage,
					verticalBlurImageInfos.GetSubView(0, 1)
				},
				DescriptorSet::UpdateInfo{
					descriptorSets[Stages::LensflareVerticalBlur],
					1,
					0,
					DescriptorType::StorageImage,
					verticalBlurImageInfos.GetSubView(1, 1)
				},
				DescriptorSet::UpdateInfo{
					descriptorSets[Stages::LensflareVerticalBlur],
					2,
					0,
					DescriptorType::StorageBuffer,
					blurBufferInfos.GetSubView(0, 1)
				},
				DescriptorSet::UpdateInfo{
					descriptorSets[Stages::Composite],
					0,
					0,
					DescriptorType::SampledImage,
					compositeImageInfos.GetSubView(0, 1)
				},
				DescriptorSet::UpdateInfo{
					descriptorSets[Stages::Composite],
					1,
					0,
					DescriptorType::SampledImage,
					compositeImageInfos.GetSubView(1, 1)
				},
				DescriptorSet::UpdateInfo{
					descriptorSets[Stages::Composite],
					2,
					0,
					DescriptorType::SampledImage,
					compositeImageInfos.GetSubView(2, 1)
				},
				DescriptorSet::UpdateInfo{
					descriptorSets[Stages::Composite],
					3,
					0,
					DescriptorType::StorageImage,
					compositeImageInfos.GetSubView(3, 1)
				},
				DescriptorSet::UpdateInfo{descriptorSets[Stages::Composite], 4, 0, DescriptorType::Sampler, compositeImageInfos.GetSubView(4, 1)},
#if ENABLE_TAA
				DescriptorSet::UpdateInfo{
					descriptorSets[Stages::TemporalAAResolve],
					0,
					0,
					DescriptorType::SampledImage,
					temporalAAResolveImageInfos.GetSubView(0, 1)
				},
				DescriptorSet::UpdateInfo{
					descriptorSets[Stages::TemporalAAResolve],
					1,
					0,
					DescriptorType::SampledImage,
					temporalAAResolveImageInfos.GetSubView(1, 1)
				},
				DescriptorSet::UpdateInfo{
					descriptorSets[Stages::TemporalAAResolve],
					2,
					0,
					DescriptorType::StorageImage,
					temporalAAResolveImageInfos.GetSubView(2, 1)
				},
				DescriptorSet::UpdateInfo{
					descriptorSets[Stages::TemporalAAResolve],
					3,
					0,
					DescriptorType::Sampler,
					temporalAAResolveImageInfos.GetSubView(3, 1)
				},
				DescriptorSet::UpdateInfo{
					descriptorSets[Stages::TemporalAAResolve],
					4,
					0,
					DescriptorType::Sampler,
					temporalAAResolveImageInfos.GetSubView(4, 1)
				},
				DescriptorSet::UpdateInfo{
					descriptorSets[Stages::TemporalAAResolve],
					5,
					0,
					DescriptorType::SampledImage,
					temporalAAResolveImageInfos.GetSubView(5, 1)
				},
#endif
#if ENABLE_FSR
				DescriptorSet::UpdateInfo(
					descriptorSets[Stages::SuperResolution],
					0,
					0,
					DescriptorType::SampledImage,
					superResolutionImageInfos.GetSubView(0, 1)
				),
				DescriptorSet::UpdateInfo(
					descriptorSets[Stages::SuperResolution],
					1,
					0,
					DescriptorType::StorageImage,
					superResolutionImageInfos.GetSubView(1, 1)
				),
				DescriptorSet::UpdateInfo(
					descriptorSets[Stages::SuperResolution],
					2,
					0,
					DescriptorType::Sampler,
					superResolutionImageInfos.GetSubView(2, 1)
				)
#endif
		};

		DescriptorSet::Update(m_sceneView.GetLogicalDevice(), descriptorUpdates);
	}

	void PostProcessStage::PopulateSharpenDescriptorSets(const DescriptorSetView descriptorSet, const ImageMappingView swapchain)
	{
		Array imageInfos{
			DescriptorSet::ImageInfo{{}, m_imageMappings[Images::SuperResolution], ImageLayout::ShaderReadOnlyOptimal},
			DescriptorSet::ImageInfo{{}, swapchain, ImageLayout::General},
			DescriptorSet::ImageInfo{m_fsrSampler, {}, ImageLayout::Undefined},
		};
		Array descriptorUpdates{
			DescriptorSet::UpdateInfo(descriptorSet, 0, 0, DescriptorType::SampledImage, imageInfos.GetSubView(0, 1)),
			DescriptorSet::UpdateInfo(descriptorSet, 1, 0, DescriptorType::StorageImage, imageInfos.GetSubView(1, 1)),
			DescriptorSet::UpdateInfo(descriptorSet, 2, 0, DescriptorType::Sampler, imageInfos.GetSubView(2, 1)),
		};
		DescriptorSet::Update(m_sceneView.GetLogicalDevice(), descriptorUpdates);
	}

	void PostProcessStage::PopulateTemporalAAResolveDescriptorSets(const DescriptorSetView descriptorSet, const ImageMappingView swapchain)
	{
		Array imageInfos{
			DescriptorSet::ImageInfo{{}, m_imageMappings[Images::Composite], ImageLayout::ShaderReadOnlyOptimal},
			DescriptorSet::ImageInfo{{}, m_imageMappings[Images::TemporalAAHistory], ImageLayout::ShaderReadOnlyOptimal},
			DescriptorSet::ImageInfo{{}, swapchain, ImageLayout::General},
			DescriptorSet::ImageInfo{m_temporalAntiAliasingCompositeSampler, {}, ImageLayout::Undefined},
			DescriptorSet::ImageInfo{m_temporalAntiAliasingHistorySampler, {}, ImageLayout::Undefined},
			DescriptorSet::ImageInfo{{}, m_imageMappings[Images::TemporalAAVelocity], ImageLayout::ShaderReadOnlyOptimal},
		};
		Array descriptorUpdates{
			DescriptorSet::UpdateInfo{descriptorSet, 0, 0, DescriptorType::SampledImage, imageInfos.GetSubView(0, 1)},
			DescriptorSet::UpdateInfo{descriptorSet, 1, 0, DescriptorType::SampledImage, imageInfos.GetSubView(1, 1)},
			DescriptorSet::UpdateInfo{descriptorSet, 2, 0, DescriptorType::StorageImage, imageInfos.GetSubView(2, 1)},
			DescriptorSet::UpdateInfo{descriptorSet, 3, 0, DescriptorType::Sampler, imageInfos.GetSubView(3, 1)},
			DescriptorSet::UpdateInfo{descriptorSet, 4, 0, DescriptorType::Sampler, imageInfos.GetSubView(4, 1)},
			DescriptorSet::UpdateInfo{descriptorSet, 5, 0, DescriptorType::SampledImage, imageInfos.GetSubView(5, 1)},
		};
		DescriptorSet::Update(m_sceneView.GetLogicalDevice(), descriptorUpdates);
	}

	void PostProcessStage::PopulateCompositeDescriptorSets(const DescriptorSetView descriptorSet, const ImageMappingView swapchain)
	{
		const Array compositeImageInfos{
			DescriptorSet::ImageInfo{{}, m_imageMappings[Images::HDR], ImageLayout::ShaderReadOnlyOptimal},
			DescriptorSet::ImageInfo{{}, m_imageMappings[Images::LensFlare], ImageLayout::ShaderReadOnlyOptimal},
			DescriptorSet::ImageInfo{{}, m_imageMappings[Images::LensDirt], ImageLayout::ShaderReadOnlyOptimal},
			DescriptorSet::ImageInfo{{}, swapchain, ImageLayout::General},
			DescriptorSet::ImageInfo{m_bilinearSampler, {}, ImageLayout::Undefined},
		};

		const Array descriptorUpdates{
			DescriptorSet::UpdateInfo{descriptorSet, 0, 0, DescriptorType::SampledImage, compositeImageInfos.GetSubView(0, 1)},
			DescriptorSet::UpdateInfo{descriptorSet, 1, 0, DescriptorType::SampledImage, compositeImageInfos.GetSubView(1, 1)},
			DescriptorSet::UpdateInfo{descriptorSet, 2, 0, DescriptorType::SampledImage, compositeImageInfos.GetSubView(2, 1)},
			DescriptorSet::UpdateInfo{descriptorSet, 3, 0, DescriptorType::StorageImage, compositeImageInfos.GetSubView(3, 1)},
			DescriptorSet::UpdateInfo{descriptorSet, 4, 0, DescriptorType::Sampler, compositeImageInfos.GetSubView(4, 1)},
		};

		DescriptorSet::Update(m_sceneView.GetLogicalDevice(), descriptorUpdates);
	}

	void PostProcessStage::OnBeforeRenderPassDestroyed()
	{
		Threading::UniqueLock lock(m_descriptorMutex);
		m_loadedAllTextures = false;
		m_loadingTextureMask |= (ImagesMask)(Images::RequiredFramegraphMask);

		m_pCompositeTexture = {};
		m_pTemporalAAHistoryTexture = {};
		m_pResolveOutputTexture = {};

		for (Images image = Images::DynamicRenderTargetBegin; image != Images::DynamicRenderTargetEnd; image = Images(ImagesMask(image) + 1))
		{
			m_imageMappings[image] = {};
		}
	}

	bool PostProcessStage::ShouldRecordCommands() const
	{
		if (m_sceneView.GetLogicalDevice().GetPhysicalDevice().GetSupportedFeatures().AreAllSet(
					PhysicalDeviceFeatures::ReadWriteBuffers | PhysicalDeviceFeatures::ReadWriteTextures
				))
		{
			return m_loadedAllTextures & m_sceneView.HasActiveCamera() & m_downsamplePipeline.IsValid() & m_lensFlarePipeline.IsValid() &
			       m_blurPipeline.IsValid() & m_compositePostProcessPipeline.IsValid() & m_temporalAntiAliasingResolvePipeline.IsValid() &
			       m_superResolutionPipeline.IsValid() & m_sharpenPipeline.IsValid() &
			       m_descriptorSets.GetView().All(
							 [](const DescriptorSetView descriptorSet)
							 {
								 return descriptorSet.IsValid();
							 }
						 );
		}
		else
		{
			return m_loadedAllTextures & m_sceneView.HasActiveCamera() & m_downsamplePipeline.IsValid() & m_lensFlarePipeline.IsValid() &
			       m_blurPipeline.IsValid() & m_compositePostProcessPipeline.IsValid() &
			       m_descriptorSets.GetView().All(
							 [](const DescriptorSetView descriptorSet)
							 {
								 return descriptorSet.IsValid();
							 }
						 );
		}
	}

	void PostProcessStage::OnAfterRecordCommands([[maybe_unused]] const CommandEncoderView commandEncoder)
	{
#if ENABLE_TAA
		const Math::Vector2ui renderResolution =
			(Math::Vector2ui)(Math::Vector2f(m_sceneView.GetDrawer().GetFullRenderResolution()) * UpscalingFactor);

		if (m_isFirstFrame)
		{
			if (m_pTemporalAAHistoryTexture.IsValid() && m_pCompositeTexture.IsValid())
			{
				{
					BarrierCommandEncoder barrierCommandEncoder = commandEncoder.BeginBarrier();

					barrierCommandEncoder.TransitionImageLayout(
						PipelineStageFlags::Transfer,
						AccessFlags::TransferWrite,
						ImageLayout::TransferDestinationOptimal,
						*m_pTemporalAAHistoryTexture,
						ImageSubresourceRange(ImageAspectFlags::Color)
					);

					barrierCommandEncoder.TransitionImageLayout(
						PipelineStageFlags::Transfer,
						AccessFlags::TransferRead,
						ImageLayout::TransferSourceOptimal,
						*m_pCompositeTexture,
						ImageSubresourceRange(ImageAspectFlags::Color)
					);
				}

				BlitCommandEncoder blitCommandEncoder = commandEncoder.BeginBlit();
				InlineVector<ImageCopy, 1> copiedRegions;
				copiedRegions.EmplaceBack(ImageCopy{
					SubresourceLayers{ImageAspectFlags::Color, 0u, {0u, 1u}},
					Math::Vector3i{Math::Zero}, // Source offset
					SubresourceLayers{ImageAspectFlags::Color, 0u, {0u, 1u}},
					Math::Vector3i{Math::Zero}, // Target offset
					Math::Vector3ui{renderResolution.x, renderResolution.y, 1}
				});

				blitCommandEncoder.RecordCopyImageToImage(
					*m_pCompositeTexture,
					ImageLayout::TransferSourceOptimal,
					*m_pTemporalAAHistoryTexture,
					ImageLayout::TransferDestinationOptimal,
					copiedRegions.GetView()
				);

				{
					BarrierCommandEncoder barrierCommandEncoder = commandEncoder.BeginBarrier();

					barrierCommandEncoder.TransitionImageLayout(
						PipelineStageFlags::ComputeShader,
						AccessFlags::ShaderRead,
						ImageLayout::ShaderReadOnlyOptimal,
						*m_pTemporalAAHistoryTexture,
						ImageSubresourceRange(ImageAspectFlags::Color)
					);

					barrierCommandEncoder.TransitionImageLayout(
						PipelineStageFlags::ComputeShader,
						AccessFlags::ShaderRead,
						ImageLayout::ShaderReadOnlyOptimal,
						*m_pCompositeTexture,
						ImageSubresourceRange(ImageAspectFlags::Color)
					);
				}
				m_isFirstFrame = false;
			}
		}
#endif

#if ENABLE_TAA
		{
			BarrierCommandEncoder barrierCommandEncoder = commandEncoder.BeginBarrier();
			if (m_pTemporalAAHistoryTexture.IsValid())
			{
				barrierCommandEncoder.TransitionImageLayout(
					PipelineStageFlags::Transfer,
					AccessFlags::TransferWrite,
					ImageLayout::TransferDestinationOptimal,
					*m_pTemporalAAHistoryTexture,
					ImageSubresourceRange(ImageAspectFlags::Color)
				);
			}
			if (m_pResolveOutputTexture.IsValid())
			{
				barrierCommandEncoder.TransitionImageLayout(
					PipelineStageFlags::Transfer,
					AccessFlags::TransferRead,
					ImageLayout::TransferSourceOptimal,
					*m_pResolveOutputTexture,
					ImageSubresourceRange(ImageAspectFlags::Color)
				);
			}
		}

		if (m_pResolveOutputTexture.IsValid() && m_pTemporalAAHistoryTexture.IsValid())
		{
			BlitCommandEncoder blitCommandEncoder = commandEncoder.BeginBlit();

			const Array copiedRegions{ImageCopy{
				SubresourceLayers{ImageAspectFlags::Color, 0u, {0u, 1u}},
				Math::Vector3i{Math::Zero}, // Source offset
				SubresourceLayers{ImageAspectFlags::Color, 0u, {0u, 1u}},
				Math::Vector3i{Math::Zero}, // Target offset
				Math::Vector3ui{renderResolution.x, renderResolution.y, 1}
			}};
			blitCommandEncoder.RecordCopyImageToImage(
				*m_pResolveOutputTexture,
				ImageLayout::TransferSourceOptimal,
				*m_pTemporalAAHistoryTexture,
				ImageLayout::TransferDestinationOptimal,
				copiedRegions.GetView()
			);
		}

		{
			BarrierCommandEncoder barrierCommandEncoder = commandEncoder.BeginBarrier();
			if (m_pTemporalAAHistoryTexture.IsValid())
			{
				barrierCommandEncoder.TransitionImageLayout(
					PipelineStageFlags::ComputeShader,
					AccessFlags::ShaderRead,
					ImageLayout::General,
					*m_pTemporalAAHistoryTexture,
					ImageSubresourceRange(ImageAspectFlags::Color)
				);
			}
			if (m_pResolveOutputTexture.IsValid())
			{
				barrierCommandEncoder.TransitionImageLayout(
					PipelineStageFlags::ComputeShader,
					AccessFlags::ShaderRead,
					ImageLayout::General,
					*m_pResolveOutputTexture,
					ImageSubresourceRange(ImageAspectFlags::Color)
				);
			}
		}
#endif
	}

	void PostProcessStage::RecordComputePassCommands(
		const ComputeCommandEncoderView computeCommandEncoder, const ViewMatrices&, [[maybe_unused]] const uint8 subpassIndex
	)
	{
		switch ((Stages)subpassIndex)
		{
			case Stages::Downsample:
			{
				const Math::Vector2ui renderResolution =
					(Math::Vector2ui)(Math::Vector2f(m_sceneView.GetDrawer().GetFullRenderResolution()) * UpscalingFactor);
				computeCommandEncoder.BindPipeline(m_downsamplePipeline);
				m_downsamplePipeline.Compute(
					ArrayView<const DescriptorSetView>(m_descriptorSets[Stages::Downsample].AtomicLoad()),
					computeCommandEncoder,
					renderResolution / LensFlareResolutionDivider
				);
			}
			break;
			case Stages::LensflareGeneration:
			{
				const Math::Vector2ui renderResolution =
					(Math::Vector2ui)(Math::Vector2f(m_sceneView.GetDrawer().GetFullRenderResolution()) * UpscalingFactor);
				computeCommandEncoder.BindPipeline(m_lensFlarePipeline);
				m_lensFlarePipeline.Compute(
					m_logicalDevice,
					ArrayView<const DescriptorSetView>(m_descriptorSets[Stages::LensflareGeneration].AtomicLoad()),
					computeCommandEncoder,
					renderResolution / LensFlareResolutionDivider
				);
			}
			break;
			case Stages::LensflareHorizontalBlur:
			{
				{
					computeCommandEncoder.BindPipeline(m_blurPipeline);
					m_blurPipeline.Compute(
						m_logicalDevice,
						ArrayView<const DescriptorSetView>(m_descriptorSets[Stages::LensflareHorizontalBlur].AtomicLoad()),
						computeCommandEncoder,
						m_sceneView.GetDrawer().GetFullRenderResolution() / PostProcessStage::LensFlareResolutionDivider,
						Math::Vector2i(1, 0)
					);
				}
			}
			break;
			case Stages::LensflareVerticalBlur:
			{
				const Math::Vector2ui renderResolution =
					(Math::Vector2ui)(Math::Vector2f(m_sceneView.GetDrawer().GetFullRenderResolution()) * UpscalingFactor);

				{
					computeCommandEncoder.BindPipeline(m_blurPipeline);
					m_blurPipeline.Compute(
						m_logicalDevice,
						ArrayView<const DescriptorSetView>(m_descriptorSets[Stages::LensflareVerticalBlur].AtomicLoad()),
						computeCommandEncoder,
						renderResolution / PostProcessStage::LensFlareResolutionDivider,
						Math::Vector2i(0, 1)
					);
				}
			}
			break;
			case Stages::Composite:
			{
#if !ENABLE_TAA
				ImageMappingView swapchainMappingView = m_sceneView.GetOutput().GetCurrentColorImageMapping();

				DescriptorSetView compositeDescriptorSet = m_descriptorSets[Stages::Composite];
				PopulateCompositeDescriptorSets(compositeDescriptorSet, swapchainMappingView);
#endif

				{
					const Math::Vector2ui renderResolution =
						(Math::Vector2ui)(Math::Vector2f(m_sceneView.GetDrawer().GetFullRenderResolution()) * UpscalingFactor);

					computeCommandEncoder.BindPipeline(m_compositePostProcessPipeline);
					m_compositePostProcessPipeline.Compute(
						m_logicalDevice,
						ArrayView<const DescriptorSetView>(m_descriptorSets[Stages::Composite].AtomicLoad()),
						computeCommandEncoder,
						renderResolution,
						Math::Vector4f{Math::Zero}
					);
				}
			}
			break;
			case Stages::TemporalAAResolve:
			{
				const Math::Vector2ui renderResolution =
					(Math::Vector2ui)(Math::Vector2f(m_sceneView.GetDrawer().GetFullRenderResolution()) * UpscalingFactor);

				{
					computeCommandEncoder.BindPipeline(m_temporalAntiAliasingResolvePipeline);
					m_temporalAntiAliasingResolvePipeline.Compute(
						m_logicalDevice,
						ArrayView<const DescriptorSetView>(m_descriptorSets[Stages::TemporalAAResolve].AtomicLoad()),
						computeCommandEncoder,
						renderResolution,
						Math::Vector4f{Math::Zero}
					);
				}
			}
			break;
			case Stages::SuperResolution:
			{
				{
					const Math::Vector2ui finalRenderResolution = m_sceneView.GetDrawer().GetFullRenderResolution();
					const Math::Vector2ui renderResolution = (Math::Vector2ui)(Math::Vector2f(finalRenderResolution) * UpscalingFactor);

					computeCommandEncoder.BindPipeline(m_superResolutionPipeline);
					m_superResolutionPipeline.ComputeEASUConstants(renderResolution, finalRenderResolution);
					m_superResolutionPipeline.Compute(
						m_logicalDevice,
						ArrayView<const DescriptorSetView>(m_descriptorSets[Stages::SuperResolution].AtomicLoad()),
						computeCommandEncoder,
						m_sceneView.GetDrawer().GetFullRenderResolution()
					);
				}
			}
			break;
			case Stages::Sharpen:
			{
				ImageMappingView swapchainMappingView = m_sceneView.GetOutput().GetCurrentColorImageMapping();

				DescriptorSetView sharpenDescriptorSet = m_descriptorSets[Stages::Sharpen];
				PopulateSharpenDescriptorSets(sharpenDescriptorSet, swapchainMappingView);

				{
					computeCommandEncoder.BindPipeline(m_sharpenPipeline);
					m_sharpenPipeline.ComputeRCASConstants();
					m_sharpenPipeline.Compute(
						m_logicalDevice,
						ArrayView<const DescriptorSetView>(sharpenDescriptorSet),
						computeCommandEncoder,
						m_sceneView.GetDrawer().GetFullRenderResolution()
					);
				}
			}
			break;
			case Stages::Count:
				ExpectUnreachable();
		}
	}

	void PostProcessStage::OnComputePassAttachmentsLoaded(
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
			case Stages::Downsample:
			{
				m_imageMappings[Images::Downsampled] = outputAttachmentMappings[0][0];
				m_imageMappings[Images::HDR] = inputAttachmentMappings[0][0];
				const ImagesMask removedMask = (ImagesMask)Images::DownsampleStageMask;
				const ImagesMask previousMask = m_loadingTextureMask.FetchAnd((ImagesMask)~removedMask);
				if ((previousMask & removedMask) != 0 && (previousMask & ~removedMask) == 0)
				{
					OnFinishedLoadingTextures();
				}
			}
			break;
			case Stages::LensflareGeneration:
			{
				m_imageMappings[Images::LensFlare] = outputAttachmentMappings[0][0];
				const ImagesMask removedMask = (ImagesMask)Images::LensFlareGenerationStageMask;
				const ImagesMask previousMask = m_loadingTextureMask.FetchAnd((ImagesMask)~removedMask);
				if ((previousMask & removedMask) != 0 && (previousMask & ~removedMask) == 0)
				{
					OnFinishedLoadingTextures();
				}
			}
			break;
			case Stages::LensflareHorizontalBlur:
				break;
			case Stages::LensflareVerticalBlur:
				break;
			case Stages::Composite:
			{
				m_imageMappings[Images::Composite] = outputAttachmentMappings[0][0];
				m_pCompositeTexture = outputAttachments[0][0];

				const ImagesMask removedMask = (ImagesMask)Images::CompositeStageMask;
				const ImagesMask previousMask = m_loadingTextureMask.FetchAnd((ImagesMask)~removedMask);
				if ((previousMask & removedMask) != 0 && (previousMask & ~removedMask) == 0)
				{
					OnFinishedLoadingTextures();
				}
			}
			break;
			case Stages::TemporalAAResolve:
			{
				m_imageMappings[Images::TemporalAAVelocity] = inputAttachmentMappings[0][2];
				m_imageMappings[Images::TemporalAAHistory] = inputAttachmentMappings[0][1];
				m_imageMappings[Images::HDR] = outputAttachmentMappings[0][0];

				m_pTemporalAAHistoryTexture = inputAttachments[0][1];
				m_pResolveOutputTexture = outputAttachments[0][0];

				const ImagesMask removedMask = (ImagesMask)Images::TemporalAAResolveStageMask;
				const ImagesMask previousMask = m_loadingTextureMask.FetchAnd((ImagesMask)~removedMask);
				if ((previousMask & removedMask) != 0 && (previousMask & ~removedMask) == 0)
				{
					OnFinishedLoadingTextures();
				}
			}
			break;
			case Stages::SuperResolution:
			{
				m_imageMappings[Images::SuperResolution] = outputAttachmentMappings[0][0];
				const ImagesMask removedMask = (ImagesMask)Images::SuperResolutionStageMask;
				const ImagesMask previousMask = m_loadingTextureMask.FetchAnd((ImagesMask)~removedMask);
				if ((previousMask & removedMask) != 0 && (previousMask & ~removedMask) == 0)
				{
					OnFinishedLoadingTextures();
				}
			}
			break;
			case Stages::Sharpen:
			{
				m_imageMappings[Images::RenderOutput] = outputAttachmentMappings[0][0];
				const ImagesMask removedMask = (ImagesMask)Images::SharpenStageMask;
				const ImagesMask previousMask = m_loadingTextureMask.FetchAnd((ImagesMask)~removedMask);
				if ((previousMask & removedMask) != 0 && (previousMask & ~removedMask) == 0)
				{
					OnFinishedLoadingTextures();
				}
			}
			break;
			case Stages::Count:
				ExpectUnreachable();
		}
	}
}
