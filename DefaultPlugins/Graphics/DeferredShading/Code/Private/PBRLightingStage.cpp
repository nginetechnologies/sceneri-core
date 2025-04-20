#include "PBRLightingStage.h"
#include "ShadowsStage.h"
#include "SSRStage.h"
#include "TilePopulationStage.h"
#include "BuildAccelerationStructureStage.h"
#include "Features.h"

#include <Engine/Threading/JobRunnerThread.h>

#include <Engine/Threading/JobManager.h>
#include <Engine/Entity/CameraComponent.h>
#include <Engine/Entity/Lights/SpotLightComponent.h>
#include <Engine/Entity/Lights/PointLightComponent.h>
#include <Engine/Entity/Lights/DirectionalLightComponent.h>
#include <Engine/Entity/Lights/EnvironmentLightComponent.h>
#include <Engine/Scene/Scene.h>

#include <Renderer/Commands/SingleUseCommandBuffer.h>
#include <Renderer/Commands/ClearValue.h>
#include <Renderer/Commands/BlitCommandEncoder.h>
#include <Renderer/Commands/BarrierCommandEncoder.h>
#include <Renderer/Commands/RenderCommandEncoder.h>
#include <Renderer/Scene/SceneView.h>
#include <Renderer/Scene/SceneData.h>
#include <Renderer/Scene/SceneViewDrawer.h>
#include <Renderer/Renderer.h>
#include <Renderer/Stages/StartFrameStage.h>
#include <Renderer/Stages/Pass.h>
#include <Renderer/Stages/MaterialsStage.h>
#include <Renderer/Wrappers/AttachmentDescription.h>
#include <Renderer/Wrappers/AttachmentReference.h>
#include <Renderer/Wrappers/SubpassDependency.h>
#include <Renderer/Wrappers/CompareOperation.h>
#include <Renderer/RenderOutput/RenderOutput.h>
#include <Renderer/Assets/Texture/RenderTexture.h>
#include <Renderer/Assets/Texture/MipMask.h>
#include <Renderer/Jobs/QueueSubmissionJob.h>
#include <DeferredShading/FSR/fsr_settings.h>

#include <Common/Threading/Jobs/AsyncJob.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/Math/Log2.h>
#include <Common/Math/Vector2/Mod.h>
#include <Common/Math/Vector2/Sign.h>
#include <Common/Memory/AddressOf.h>
#include <Common/System/Query.h>

namespace ngine::Rendering
{
	inline static constexpr uint32 IrradianceRenderTargetSize = 32u;
	inline static constexpr uint32 PrefilteredEnvironmentRenderTargetSize = 256u;
	inline static constexpr uint32 BRDFRenderTargetSize = 256u;

	inline static constexpr Array<Asset::Guid, PBRLightingStage::TotalSampledTextureCount> StaticRenderTargets{
		"1FDABA46-23FD-4A86-A57B-A152808AD0F1"_asset, // BRDF
		//"04884085-d1a3-4f51-a699-28314554510e"_asset, // DefaultIrradiance
	  //"82f0d28c-a03b-49d4-a269-cdabe3769b38"_asset, // DefaultPrefilteredEnvironment
		TilePopulationStage::ClustersTextureAssetGuid,
		"90D3D032-2E1E-4D72-B3C0-D3B4114C9D2E"_asset,         // GBufferAlbedo
		"18988BCE-DD7A-4D4A-9606-741047372C44"_asset,         // GBufferWorldNormals
		"FDA607DC-ED70-4417-A242-84C3F6A86377"_asset,         // GBufferMaterialProperties
		SceneViewDrawer::DefaultDepthStencilRenderTargetGuid, // depth stencil
		ShadowsStage::RenderTargetGuid,
		"A5FA9CC6-EAFE-4F12-84D6-316532473745"_asset, // Irradiance
		"9046FE26-B623-4D56-A565-4B2A3EA40FDF"_asset, // Prefiltered Environment
	};

	inline static constexpr Array<ImageMappingType, PBRLightingStage::TotalSampledTextureCount> StaticRenderTargetMappingTypes
	{
		ImageMappingType::TwoDimensional,
			// ImageMappingType::TwoDimensional,
		  // ImageMappingType::TwoDimensional,
			ImageMappingType::TwoDimensional, ImageMappingType::TwoDimensional, ImageMappingType::TwoDimensional,
			ImageMappingType::TwoDimensional, ImageMappingType::TwoDimensional, ImageMappingType::TwoDimensionalArray,
#if SUPPORT_CUBEMAP_ARRAYS
			ImageMappingType::CubeArray, ImageMappingType::CubeArray,
#else
			ImageMappingType::Cube, ImageMappingType::Cube
#endif
	};

	inline static constexpr Array<Math::Vector2ui, PBRLightingStage::TotalSampledTextureCount> StaticRenderTargetResolutions{
		Math::Vector2ui{BRDFRenderTargetSize},
		// Math::Vector2ui{IrradianceRenderTargetSize},
	  // Math::Vector2ui{PrefilteredEnvironmentRenderTargetSize},
		Math::Zero,
		Math::Zero,
		Math::Zero,
		Math::Zero,
		Math::Zero,
		Math::Vector2ui{ShadowsStage::ShadowMapSize},
		Math::Vector2ui{IrradianceRenderTargetSize},
		Math::Vector2ui{PrefilteredEnvironmentRenderTargetSize},
	};

	[[nodiscard]] MipMask GetPrefilteredEnvironmentMipMask()
	{
		MipMask mipMask = MipMask::FromSizeAllToLargest(Math::Vector2ui{PrefilteredEnvironmentRenderTargetSize});
		// Make sure minimum is 8x8
		mipMask &= ~MipMask::FromIndex(0);
		mipMask &= ~MipMask::FromIndex(1);
		mipMask &= ~MipMask::FromIndex(2);
		return mipMask;
	}

	inline static const Array<MipMask, PBRLightingStage::TotalSampledTextureCount> StaticRenderTargetMipMasks{
		MipMask::FromSizeToLargest(Math::Vector2ui{BRDFRenderTargetSize}),
		// MipMask::FromSizeToLargest(Math::Vector2ui{ IrradianceRenderTargetSize }),
	  // GetPrefilteredEnvironmentMipMask(),
		MipMask{},
		MipMask{},
		MipMask{},
		MipMask{},
		MipMask{},
		MipMask{},
		MipMask::FromSizeToLargest(Math::Vector2ui{IrradianceRenderTargetSize}),
		GetPrefilteredEnvironmentMipMask(),
	};

	PBRLightingStage::PBRLightingStage(
		SceneView& sceneView,
		const Optional<ShadowsStage*> pShadowsStage,
		TilePopulationStage& tilePopulationStage,
		const Optional<BuildAccelerationStructureStage*> pBuildAccelerationStructureStage
	)
		: SceneRenderStage(sceneView.GetLogicalDevice(), Threading::JobPriority::Draw)
		, m_sceneView(sceneView)
		, m_pShadowsStage(pShadowsStage)
		, m_tilePopulationStage(tilePopulationStage)
		, m_pBuildAccelerationStructureStage(pBuildAccelerationStructureStage)
		, m_rasterizedPipeline(
				sceneView.GetLogicalDevice(),
				sceneView.GetMatrices().GetDescriptorSetLayout(),
				System::Get<Rendering::Renderer>().GetTextureCache(),
				System::Get<Rendering::Renderer>().GetMeshCache()
			)
		, m_raytracedPipeline(
				sceneView.GetLogicalDevice().GetPhysicalDevice().GetSupportedFeatures().AreAllSet(PhysicalDeviceFeatures::AccelerationStructure | PhysicalDeviceFeatures::RayQuery) ? PBRLightingPipelineRaytraced { sceneView.GetLogicalDevice(),
				sceneView.GetMatrices().GetDescriptorSetLayout(),
				System::Get<Rendering::Renderer>().GetTextureCache(),
				System::Get<Rendering::Renderer>().GetMeshCache() }
				: PBRLightingPipelineRaytraced{}
			)
	{
		const SceneRenderStageIdentifier stageIdentifier = System::Get<Rendering::Renderer>().GetStageCache().FindIdentifier(Guid);
		m_sceneView.RegisterSceneRenderStage(stageIdentifier, *this);
		m_sceneView.SetStageDependentOnCameraProperties(stageIdentifier);

		tilePopulationStage.SetPBRLightingStage(*this);

		for (Sampler& sampler : m_samplers)
		{
			const uint8 samplerIndex = m_samplers.GetIteratorIndex(Memory::GetAddressOf(sampler));
			switch ((SampledTextures)samplerIndex)
			{
					/*case SampledTextures::DefaultIrradiance:
					case SampledTextures::DefaultPrefilteredEnvironment:
					    break;*/

				case SampledTextures::BRDF:
					sampler = Sampler(m_sceneView.GetLogicalDevice(), AddressMode::ClampToEdge, FilterMode::Linear, CompareOperation::AlwaysSucceed);
					break;
				case SampledTextures::Clusters:
					sampler = Sampler(m_sceneView.GetLogicalDevice(), AddressMode::Repeat, FilterMode::Nearest, CompareOperation::AlwaysSucceed);
					break;
				case SampledTextures::Albedo:
				case SampledTextures::Normals:
				case SampledTextures::MaterialProperties:

#if !ENABLE_DEFERRED_LIGHTING_SUBPASSES
					sampler = Sampler(m_sceneView.GetLogicalDevice(), AddressMode::ClampToEdge, FilterMode::Linear, CompareOperation::AlwaysSucceed);
#endif
					break;
				case SampledTextures::Depth:
#if !ENABLE_DEFERRED_LIGHTING_SUBPASSES
					sampler = Sampler(m_sceneView.GetLogicalDevice(), AddressMode::ClampToEdge, FilterMode::Nearest, CompareOperation::AlwaysSucceed);
#endif
					break;
				case SampledTextures::ShadowmapArray:
				{
					if (m_pShadowsStage.IsInvalid() || m_pShadowsStage->GetState() == ShadowsStage::State::Rasterized)
					{
						sampler = Sampler(
							m_sceneView.GetLogicalDevice(),
#if PLATFORM_APPLE_VISIONOS
							AddressMode::ClampToEdge,
#else
							AddressMode::ClampToBorder,
#endif
							FilterMode::Linear,
							CompareOperation::Less
						);
					}
				}
				break;
				case SampledTextures::IrradianceArray:
				case SampledTextures::PrefilteredEnvironmentArray:
					sampler = Sampler(m_sceneView.GetLogicalDevice(), AddressMode::Repeat, FilterMode::Linear, CompareOperation::AlwaysSucceed);
					break;
				case SampledTextures::DynamicRenderTargetEnd:
				case SampledTextures::RaytracedAttachmentsMask:
				case SampledTextures::RasterizedAttachmentsMask:
				case SampledTextures::StaticAttachmentsMask:
					ExpectUnreachable();
			}
		}
	}

	PBRLightingStage::~PBRLightingStage()
	{
		m_rasterizedPipeline.Destroy(m_sceneView.GetLogicalDevice());
		m_raytracedPipeline.Destroy(m_sceneView.GetLogicalDevice());

		if (m_pDescriptorSetLoadingThread != nullptr)
		{
			m_pDescriptorSetLoadingThread->GetRenderData().DestroyDescriptorSet(m_logicalDevice.GetIdentifier(), Move(m_descriptorSet));
		}

		TextureCache& textureCache = System::Get<Rendering::Renderer>().GetTextureCache();
		RenderTargetCache& renderTargetCache = m_sceneView.GetOutput().GetRenderTargetCache();

		for (Sampler& sampler : m_samplers)
		{
			sampler.Destroy(m_sceneView.GetLogicalDevice());
		}

		for (uint8 i = (uint8)SampledTextures::StaticRenderTargetBegin; i < (uint8)SampledTextures::StaticRenderTargetEnd; ++i)
		{
			const RenderTargetTemplateIdentifier renderTargetTemplateIdentifier =
				textureCache.FindRenderTargetTemplateIdentifier(Rendering::StaticRenderTargets[i]);
			if (renderTargetTemplateIdentifier.IsValid())
			{
				textureCache.RemoveRenderTextureListener(
					m_sceneView.GetLogicalDevice().GetIdentifier(),
					renderTargetCache.FindRenderTargetFromTemplateIdentifier(renderTargetTemplateIdentifier),
					this
				);
			}
		}
		for (uint8 i = (uint8)SampledTextures::TextureBegin; i < (uint8)SampledTextures::TextureEnd; ++i)
		{
			const Rendering::TextureIdentifier textureIdentifier = textureCache.FindOrRegisterAsset(Rendering::StaticRenderTargets[i]);
			if (textureIdentifier.IsValid())
			{
				textureCache.RemoveRenderTextureListener(m_sceneView.GetLogicalDevice().GetIdentifier(), textureIdentifier, this);
			}
		}

		for (uint8 i = (uint8)SampledTextures::DynamicRenderTargetBegin; i < (uint8)SampledTextures::DynamicRenderTargetEnd; ++i)
		{
			const Rendering::RenderTargetTemplateIdentifier templateTextureIdentifier =
				textureCache.FindOrRegisterRenderTargetTemplate(Rendering::StaticRenderTargets[i]);
			if (templateTextureIdentifier.IsValid())
			{
				const Rendering::TextureIdentifier textureIdentifier =
					renderTargetCache.FindOrRegisterRenderTargetFromTemplateIdentifier(textureCache, templateTextureIdentifier);
				textureCache.RemoveRenderTextureListener(m_sceneView.GetLogicalDevice().GetIdentifier(), textureIdentifier, this);
			}
		}

		for (ImageMapping& imageMapping : m_ownedImageMappings)
		{
			imageMapping.Destroy(m_sceneView.GetLogicalDevice());
		}

		for (Rendering::TextureIdentifier textureIdentifier : m_loadedIrradianceTextures)
		{
			if (textureIdentifier.IsValid())
			{
				textureCache.RemoveRenderTextureListener(m_sceneView.GetLogicalDevice().GetIdentifier(), textureIdentifier, this);
			}
		}

		for (Rendering::TextureIdentifier textureIdentifier : m_loadedPrefilteredTextures)
		{
			if (textureIdentifier.IsValid())
			{
				textureCache.RemoveRenderTextureListener(m_sceneView.GetLogicalDevice().GetIdentifier(), textureIdentifier, this);
			}
		}

		Rendering::StageCache& stageCache = System::Get<Rendering::Renderer>().GetStageCache();
		const SceneRenderStageIdentifier stageIdentifier = stageCache.FindIdentifier(Guid);
		m_sceneView.DeregisterSceneRenderStage(stageIdentifier);
	}

	Threading::JobBatch PBRLightingStage::LoadFixedResources()
	{
		// TODO: Move all of this to the framegraph definition.

		Threading::JobBatch batch;

		TextureCache& textureCache = System::Get<Rendering::Renderer>().GetTextureCache();
		RenderTargetCache& renderTargetCache = m_sceneView.GetOutput().GetRenderTargetCache();

		Assert(m_loadingTexturesMask.Load() == 0);

		if (m_pShadowsStage.IsInvalid() || m_pShadowsStage->GetState() == ShadowsStage::State::Rasterized)
		{
			m_loadingTexturesMask = (SampledTexturesMaskType)SampledTextures::RasterizedAttachmentsMask;
		}
		else
		{
			m_loadingTexturesMask = (SampledTexturesMaskType)SampledTextures::RaytracedAttachmentsMask;
		}

		m_loadingTexturesMask |= (SampledTexturesMaskType)SampledTextures::StaticAttachmentsMask;

		for (uint8 i = (uint8)SampledTextures::TextureBegin; i < (uint8)SampledTextures::TextureEnd; ++i)
		{
			const MipMask mipMask = Rendering::StaticRenderTargetMipMasks[i];
			Threading::Job* pTextureJob = textureCache.GetOrLoadRenderTexture(
				m_sceneView.GetLogicalDevice().GetIdentifier(),
				textureCache.FindOrRegisterAsset(Rendering::StaticRenderTargets[i]),
				ImageMappingType::TwoDimensional,
				mipMask,
				TextureLoadFlags::Default & ~TextureLoadFlags::LoadDummy,
				TextureCache::TextureLoadListenerData(
					*this,
					[this,
			     textureIndex =
			       i](PBRLightingStage&, const LogicalDevice& logicalDevice, const TextureIdentifier, RenderTexture& texture, const MipMask, const EnumFlags<LoadedTextureFlags>)
						-> EventCallbackResult
					{
						if (texture.GetLoadedMipCount() == Rendering::StaticRenderTargetMipMasks[textureIndex].GetSize())
						{
							OnTextureLoaded(textureIndex, logicalDevice, texture);

							switch ((SampledTextures)textureIndex)
							{
								case SampledTextures::IrradianceArray:
								case SampledTextures::PrefilteredEnvironmentArray:
								{
									// Load environment light textures that were waiting
									const ArrayView<const ReferenceWrapper<const Entity::LightSourceComponent>> visibleEnvironmentLights =
										m_tilePopulationStage.GetVisibleLights(LightTypes::EnvironmentLight);
									for (const Entity::RenderItemIdentifier& visibleEnvironmentLight : m_visibleEnvironmentLights)
									{
										const OptionalIterator<const ReferenceWrapper<const Entity::LightSourceComponent>> pEnvironmentLightIterator =
											visibleEnvironmentLights.FindIf(
												[visibleEnvironmentLight](const Entity::LightSourceComponent& light)
												{
													return light.GetRenderItemIdentifier() == visibleEnvironmentLight;
												}
											);
										if (LIKELY(pEnvironmentLightIterator.IsValid()))
										{
											LoadEnvironmentLightTextures(
												static_cast<const Entity::EnvironmentLightComponent&>(**pEnvironmentLightIterator),
												m_visibleEnvironmentLights.GetIteratorIndex(&visibleEnvironmentLight)
											);
										}
									}
								}
								break;
								default:
									break;
							}
						}
						return EventCallbackResult::Keep;
					}
				)
			);
			if (pTextureJob != nullptr)
			{
				batch.QueueAfterStartStage(*pTextureJob);
			}
		}

		for (uint8 i = (uint8)SampledTextures::DynamicRenderTargetBegin; i < (uint8)SampledTextures::DynamicRenderTargetEnd; ++i)
		{
			Math::Vector2ui renderTargetResolution = Rendering::StaticRenderTargetResolutions[i];
			Assert((renderTargetResolution == Math::Zero).AreNoneSet());

			MipMask mipMask = Rendering::StaticRenderTargetMipMasks[i];
			if (!mipMask.AreAnySet())
			{
				mipMask = MipMask::FromSizeAllToLargest(renderTargetResolution);
			}
			const Rendering::RenderTargetTemplateIdentifier templateTextureIdentifier =
				textureCache.FindOrRegisterRenderTargetTemplate(Rendering::StaticRenderTargets[i]);
			Threading::Job* pRenderTargetJob = textureCache.GetOrLoadRenderTarget(
				m_sceneView.GetLogicalDevice(),
				renderTargetCache.FindOrRegisterRenderTargetFromTemplateIdentifier(textureCache, templateTextureIdentifier),
				templateTextureIdentifier,
				SampleCount::One,
				renderTargetResolution,
				mipMask,
				ArrayRange{0, MaximumEnvironmentLightCount * 6},
				TextureCache::TextureLoadListenerData{
					*this,
					[this,
			     textureIndex = i,
			     mipMask](PBRLightingStage&, const LogicalDevice& logicalDevice, const Rendering::TextureIdentifier, RenderTexture& texture, const MipMask, const EnumFlags<LoadedTextureFlags>)
						-> EventCallbackResult
					{
						// Transition the pipeline barrier
				    // TODO: Make the framegraph take charge of these attachments
						if (Ensure(texture.IsValid()))
						{
							Threading::EngineJobRunnerThread& thread = *Threading::EngineJobRunnerThread::GetCurrent();
							UnifiedCommandBuffer commandBuffer(
								m_logicalDevice,
								thread.GetRenderData().GetCommandPool(m_logicalDevice.GetIdentifier(), Rendering::QueueFamily::Graphics),
								m_logicalDevice.GetCommandQueue(Rendering::QueueFamily::Graphics)
							);
							CommandEncoderView commandEncoder = commandBuffer.BeginEncoding(m_logicalDevice);

							{
								BarrierCommandEncoder barrierCommandEncoder = commandEncoder.BeginBarrier();

								barrierCommandEncoder.TransitionImageLayout(
									PipelineStageFlags::FragmentShader,
									AccessFlags::ShaderRead,
									ImageLayout::ShaderReadOnlyOptimal,
									texture,
									Rendering::ImageSubresourceRange{
										ImageAspectFlags::Color,
										MipRange{0, mipMask.GetSize()},
										ArrayRange{0, MaximumEnvironmentLightCount * 6}
									}
								);
							}

							const EncodedCommandBufferView encodedCommandBuffer = commandBuffer.StopEncoding();

							QueueSubmissionParameters parameters;
							parameters.m_finishedCallback =
								[this, textureIndex, &logicalDevice, &texture, commandBuffer = Move(commandBuffer), &thread]() mutable
							{
								OnTextureLoaded(textureIndex, logicalDevice, texture);

								thread.QueueExclusiveCallbackFromAnyThread(
									Threading::JobPriority::DeallocateResourcesMin,
									[commandBuffer = Move(commandBuffer), &logicalDevice](Threading::JobRunnerThread& thread) mutable
									{
										Threading::EngineJobRunnerThread& engineThread = static_cast<Threading::EngineJobRunnerThread&>(thread);
										commandBuffer.Destroy(
											logicalDevice,
											engineThread.GetRenderData().GetCommandPool(logicalDevice.GetIdentifier(), Rendering::QueueFamily::Graphics)
										);
									}
								);
							};

							m_logicalDevice.GetQueueSubmissionJob(QueueFamily::Graphics)
								.Queue(
									Threading::JobPriority::CreateRenderTargetSubmission,
									ArrayView<const EncodedCommandBufferView, uint16>(encodedCommandBuffer),
									Move(parameters)
								);
						}

						return EventCallbackResult::Keep;
					}
				}
			);
			if (pRenderTargetJob != nullptr)
			{
				batch.QueueAfterStartStage(*pRenderTargetJob);
			}
		}

		return batch;
	}

	void PBRLightingStage::OnTextureLoaded(const uint8 textureIndex, const LogicalDevice& logicalDevice, RenderTexture& texture)
	{
		switch ((SampledTextures)textureIndex)
		{
				/*case SampledTextures::DefaultIrradiance:
				case SampledTextures::DefaultPrefilteredEnvironment:
				{
				  const SampledTexturesMaskType loadedTextureMask = SampledTexturesMaskType(1u << textureIndex);
				  const SampledTexturesMaskType previousLoadingTexturesMask = m_loadingTexturesMask.FetchAnd(~loadedTextureMask);
				  Assert((previousLoadingTexturesMask & loadedTextureMask) == previousLoadingTexturesMask);
				  if (previousLoadingTexturesMask == previousLoadingTexturesMask)
				  {
				    OnTexturesLoaded();
				  }
				}
				break;*/

			case SampledTextures::BRDF:
			case SampledTextures::IrradianceArray:
			case SampledTextures::PrefilteredEnvironmentArray:
			{
				const ImageMappingType mappingType = Rendering::StaticRenderTargetMappingTypes[textureIndex];
				EnumFlags<ImageAspectFlags> aspectFlags;
				if (textureIndex == (uint8)SampledTextures::ShadowmapArray || textureIndex == (uint8)SampledTextures::Depth)
				{
					aspectFlags |= ImageAspectFlags::Depth;
				}
				else
				{
					aspectFlags |= ImageAspectFlags::Color;
				}

				m_imageMappingsViews[textureIndex] = m_ownedImageMappings[textureIndex] = ImageMapping(
					logicalDevice,
					texture,
					mappingType,
					texture.GetFormat(),
					aspectFlags,
					MipRange{0, texture.GetTotalMipCount()},
					ArrayRange{0, texture.GetTotalArrayCount()}
				);

				if (LIKELY(m_imageMappingsViews[textureIndex].IsValid()))
				{
					const SampledTexturesMaskType loadedTextureMask = SampledTexturesMaskType(1u << textureIndex);
					const SampledTexturesMaskType previousLoadingTexturesMask = m_loadingTexturesMask.FetchAnd((SampledTexturesMaskType
					)~loadedTextureMask);
					Assert((previousLoadingTexturesMask & loadedTextureMask) == loadedTextureMask);
					if (previousLoadingTexturesMask == loadedTextureMask)
					{
						OnTexturesLoaded();
					}
				}
			}
			break;
			case SampledTextures::Albedo:
			case SampledTextures::Normals:
			case SampledTextures::MaterialProperties:
			case SampledTextures::Depth:
			case SampledTextures::Clusters:
			case SampledTextures::ShadowmapArray:
			case SampledTextures::RasterizedCount:
			case SampledTextures::RaytracedAttachmentsMask:
			case SampledTextures::RasterizedAttachmentsMask:
			case SampledTextures::StaticAttachmentsMask:
				ExpectUnreachable();
		}
	}

	void PBRLightingStage::OnTexturesLoaded()
	{
		const Optional<Threading::EngineJobRunnerThread*> pEngineThread = Threading::EngineJobRunnerThread::GetCurrent();
		if (pEngineThread.IsInvalid())
		{
			System::Get<Threading::JobManager>().QueueCallback(
				[this](Threading::JobRunnerThread&)
				{
					OnTexturesLoaded();
				},
				Threading::JobPriority::CoreRenderStageResources
			);
			return;
		}

		{
			Array<DescriptorSet, 1> descriptorSets;
			[[maybe_unused]] const bool allocatedDescriptorSets = pEngineThread->GetRenderData()
			                                                        .GetDescriptorPool(m_sceneView.GetLogicalDevice().GetIdentifier())
			                                                        .AllocateDescriptorSets(
																																m_sceneView.GetLogicalDevice(),
																																Array<const DescriptorSetLayoutView, 1>(
																																	m_pShadowsStage.IsInvalid() ||
																																			m_pShadowsStage->GetState() == ShadowsStage::State::Rasterized
																																		? (DescriptorSetLayoutView)m_rasterizedPipeline
																																		: (DescriptorSetLayoutView)m_raytracedPipeline
																																),
																																descriptorSets
																															);
			Assert(allocatedDescriptorSets);
			if (LIKELY(allocatedDescriptorSets))
			{
				Threading::JobRunnerThread* pPreviousDescriptorSetLoadingThread = m_pDescriptorSetLoadingThread;

				{
					Threading::UniqueLock lock(m_textureLoadMutex);
					PopulateDescriptorSet(descriptorSets[0]);
				}

				m_shouldUpdateLightBuffer = true;
				m_descriptorSet.AtomicSwap(descriptorSets[0]);
				m_pDescriptorSetLoadingThread = pEngineThread;

				if (pPreviousDescriptorSetLoadingThread != nullptr)
				{
					Threading::EngineJobRunnerThread& previousEngineThread =
						static_cast<Threading::EngineJobRunnerThread&>(*pPreviousDescriptorSetLoadingThread);
					previousEngineThread.GetRenderData().DestroyDescriptorSet(m_logicalDevice.GetIdentifier(), Move(descriptorSets[0]));
				}
			}
		}

		m_loadedAllTextures = true;
	}

	void PBRLightingStage::PopulateDescriptorSet(const Rendering::DescriptorSetView descriptorSet)
	{
		FlatVector<DescriptorSet::ImageInfo, TotalSampledTextureCount * 2> imageInfo;
		FlatVector<DescriptorSet::BufferInfo, (uint8)LightTypes::Count - 1 + 2> bufferInfo;
		FlatVector<DescriptorSet::UpdateInfo, (uint8)PBRLightingPipeline::DescriptorBinding::Count> descriptorUpdates;

		if (m_pShadowsStage.IsInvalid() || m_pShadowsStage->GetState() == ShadowsStage::State::Rasterized)
		{
			[[maybe_unused]] auto emplaceInputAttachment = [this, &imageInfo, &descriptorUpdates, descriptorSet](
																											 const SampledTextures texture,
																											 const PBRLightingPipeline::DescriptorBinding descriptorBinding,
																											 const ImageLayout imageLayout
																										 )
			{
				const uint8 index = (uint8)texture;
				Assert(m_imageMappingsViews[index].IsValid());

				DescriptorSet::ImageInfo& emplacedImageInfo =
					imageInfo.EmplaceBack(DescriptorSet::ImageInfo{m_samplers[index], m_imageMappingsViews[index], imageLayout});

				descriptorUpdates.EmplaceBack(
					descriptorSet,
					(uint8)descriptorBinding,
					0,
					DescriptorType::InputAttachment,
					ArrayView<const DescriptorSet::ImageInfo>(emplacedImageInfo)
				);
			};

			auto emplaceSampledTexture = [this, &imageInfo, &descriptorUpdates, descriptorSet](
																		 const SampledTextures texture,
																		 const PBRLightingPipeline::DescriptorBinding descriptorBinding,
																		 const ImageLayout imageLayout
																	 )
			{
				const uint8 index = (uint8)texture;
				Assert(m_imageMappingsViews[index].IsValid());

				DescriptorSet::ImageInfo& emplacedImageInfo =
					imageInfo.EmplaceBack(DescriptorSet::ImageInfo{{}, m_imageMappingsViews[index], imageLayout});

				descriptorUpdates.EmplaceBack(
					descriptorSet,
					(uint8)descriptorBinding,
					0,
					DescriptorType::SampledImage,
					ArrayView<const DescriptorSet::ImageInfo>(emplacedImageInfo)
				);
			};

			auto emplaceSampler = [this, &imageInfo, &descriptorUpdates, descriptorSet](
															const SampledTextures texture,
															const PBRLightingPipeline::DescriptorBinding descriptorBinding,
															const ImageLayout imageLayout
														)
			{
				const uint8 index = (uint8)texture;
				Assert(m_samplers[index].IsValid());

				DescriptorSet::ImageInfo& emplacedImageInfo = imageInfo.EmplaceBack(DescriptorSet::ImageInfo{m_samplers[index], {}, imageLayout});

				descriptorUpdates.EmplaceBack(
					descriptorSet,
					(uint8)descriptorBinding,
					0,
					DescriptorType::Sampler,
					ArrayView<const DescriptorSet::ImageInfo>(emplacedImageInfo)
				);
			};

			auto emplaceLightBuffer = [this, &bufferInfo, &descriptorUpdates, descriptorSet](
																	const LightTypes lightType,
																	const PBRLightingPipeline::DescriptorBinding descriptorBinding,
																	const size structBaseSize,
																	const size structSize,
																	const uint32 maximumLightCount
																)
			{
				const FixedArrayView<const StorageBuffer, (uint8)LightTypes::Count - 1> lightStorageBuffers =
					m_tilePopulationStage.GetLightStorageBuffers();
				const StorageBuffer& lightBuffer = lightStorageBuffers[(uint8)lightType];
				const uint8 index = (uint8)lightType;
				const ArrayView<const ReferenceWrapper<const Entity::LightSourceComponent>> visibleLights =
					m_tilePopulationStage.GetVisibleLights((LightTypes)index);

				DescriptorSet::BufferInfo& emplacedBufferInfo = bufferInfo.EmplaceBack(DescriptorSet::BufferInfo{
					lightBuffer,
					0,
					structBaseSize + Math::Max(structSize * Math::Min(visibleLights.GetSize(), maximumLightCount), structSize)
				});

				descriptorUpdates.EmplaceBack(
					descriptorSet,
					(uint8)descriptorBinding,
					0,
					DescriptorType::StorageBuffer,
					ArrayView<const DescriptorSet::BufferInfo>(emplacedBufferInfo)
				);
			};

			for (PBRLightingPipeline::DescriptorBinding descriptorBinding = PBRLightingPipeline::DescriptorBinding::First;
			     descriptorBinding < PBRLightingPipeline::DescriptorBinding::Count;
			     descriptorBinding = PBRLightingPipeline::DescriptorBinding((uint8)descriptorBinding + 1))
			{
				switch (descriptorBinding)
				{
					case PBRLightingPipeline::DescriptorBinding::BRDFTexture:
						emplaceSampledTexture(SampledTextures::BRDF, descriptorBinding, ImageLayout::ShaderReadOnlyOptimal);
						break;
					case PBRLightingPipeline::DescriptorBinding::BRDFSampler:
						emplaceSampler(SampledTextures::BRDF, descriptorBinding, ImageLayout::ShaderReadOnlyOptimal);
						break;
					case PBRLightingPipeline::DescriptorBinding::TilesTexture:
						emplaceSampledTexture(SampledTextures::Clusters, descriptorBinding, ImageLayout::ShaderReadOnlyOptimal);
						break;
					case PBRLightingPipeline::DescriptorBinding::TilesSampler:
						emplaceSampler(SampledTextures::Clusters, descriptorBinding, ImageLayout::ShaderReadOnlyOptimal);
						break;

#if ENABLE_DEFERRED_LIGHTING_SUBPASSES
					case PBRLightingPipeline::DescriptorBinding::AlbedoTexture:
						emplaceInputAttachment(SampledTextures::Albedo, descriptorBinding, ImageLayout::ShaderReadOnlyOptimal);
						break;
					case PBRLightingPipeline::DescriptorBinding::NormalsTexture:
						emplaceInputAttachment(SampledTextures::Normals, descriptorBinding, ImageLayout::ShaderReadOnlyOptimal);
						break;
					case PBRLightingPipeline::DescriptorBinding::MaterialPropertiesTexture:
						emplaceInputAttachment(SampledTextures::MaterialProperties, descriptorBinding, ImageLayout::ShaderReadOnlyOptimal);
						break;
					case PBRLightingPipeline::DescriptorBinding::DepthTexture:
						emplaceInputAttachment(SampledTextures::Depth, descriptorBinding, ImageLayout::DepthStencilReadOnlyOptimal);
						break;
#else
					case PBRLightingPipeline::DescriptorBinding::AlbedoTexture:
						emplaceSampledTexture(SampledTextures::Albedo, descriptorBinding, ImageLayout::ShaderReadOnlyOptimal);
						break;
					case PBRLightingPipeline::DescriptorBinding::AlbedoSampler:
						emplaceSampler(SampledTextures::Albedo, descriptorBinding, ImageLayout::ShaderReadOnlyOptimal);
						break;
					case PBRLightingPipeline::DescriptorBinding::NormalsTexture:
						emplaceSampledTexture(SampledTextures::Normals, descriptorBinding, ImageLayout::ShaderReadOnlyOptimal);
						break;
					case PBRLightingPipeline::DescriptorBinding::NormalsSampler:
						emplaceSampler(SampledTextures::Normals, descriptorBinding, ImageLayout::ShaderReadOnlyOptimal);
						break;
					case PBRLightingPipeline::DescriptorBinding::MaterialPropertiesTexture:
						emplaceSampledTexture(SampledTextures::MaterialProperties, descriptorBinding, ImageLayout::ShaderReadOnlyOptimal);
						break;
					case PBRLightingPipeline::DescriptorBinding::MaterialPropertiesSampler:
						emplaceSampler(SampledTextures::MaterialProperties, descriptorBinding, ImageLayout::ShaderReadOnlyOptimal);
						break;
					case PBRLightingPipeline::DescriptorBinding::DepthTexture:
						emplaceSampledTexture(SampledTextures::Depth, descriptorBinding, ImageLayout::DepthStencilReadOnlyOptimal);
						break;
#endif

					case PBRLightingPipeline::DescriptorBinding::ShadowMapArrayTexture:
						emplaceSampledTexture(SampledTextures::ShadowmapArray, descriptorBinding, ImageLayout::DepthStencilReadOnlyOptimal);
						break;
					case PBRLightingPipeline::DescriptorBinding::ShadowMapArraySampler:
						emplaceSampler(SampledTextures::ShadowmapArray, descriptorBinding, ImageLayout::DepthStencilReadOnlyOptimal);
						break;
					case PBRLightingPipeline::DescriptorBinding::IrradianceTexture:
						emplaceSampledTexture(SampledTextures::IrradianceArray, descriptorBinding, ImageLayout::ShaderReadOnlyOptimal);
						break;
					case PBRLightingPipeline::DescriptorBinding::IrradianceSampler:
						emplaceSampler(SampledTextures::IrradianceArray, descriptorBinding, ImageLayout::ShaderReadOnlyOptimal);
						break;
					case PBRLightingPipeline::DescriptorBinding::PrefilteredMapTexture:
						emplaceSampledTexture(SampledTextures::PrefilteredEnvironmentArray, descriptorBinding, ImageLayout::ShaderReadOnlyOptimal);
						break;
					case PBRLightingPipeline::DescriptorBinding::PrefilteredMapSampler:
						emplaceSampler(SampledTextures::PrefilteredEnvironmentArray, descriptorBinding, ImageLayout::ShaderReadOnlyOptimal);
						break;
					case PBRLightingPipeline::DescriptorBinding::PointLightsBuffer:
						emplaceLightBuffer(
							LightTypes::PointLight,
							descriptorBinding,
							LightHeaderSizes[(uint8)LightTypes::PointLight],
							LightStructSizes[(uint8)LightTypes::PointLight],
							(uint32)MaximumLightCounts[(uint8)LightTypes::PointLight]
						);
						break;
					case PBRLightingPipeline::DescriptorBinding::SpotLightsBuffer:
						emplaceLightBuffer(
							LightTypes::SpotLight,
							descriptorBinding,
							LightHeaderSizes[(uint8)LightTypes::SpotLight],
							LightStructSizes[(uint8)LightTypes::SpotLight],
							(uint32)MaximumLightCounts[(uint8)LightTypes::SpotLight]
						);
						break;
					case PBRLightingPipeline::DescriptorBinding::DirectionalLightsBuffer:
						emplaceLightBuffer(
							LightTypes::DirectionalLight,
							descriptorBinding,
							LightHeaderSizes[(uint8)LightTypes::DirectionalLight],
							LightStructSizes[(uint8)LightTypes::DirectionalLight],
							(uint32)MaximumLightCounts[(uint8)LightTypes::DirectionalLight]
						);
						break;
#if ENABLE_SAMPLE_DISTRIBUTION_SHADOW_MAPS
					case PBRLightingPipeline::DescriptorBinding::ShadowInfoBuffer:
					{
						DescriptorSet::BufferInfo& emplacedBufferInfo = bufferInfo.EmplaceBack(DescriptorSet::BufferInfo{
							m_pShadowsStage->GetSDSMShadowSamplingInfoBuffer(),
							0,
							m_pShadowsStage->GetSDSMShadowSamplingInfoBufferSize()
						});

						descriptorUpdates.EmplaceBack(
							descriptorSet,
							(uint8)descriptorBinding,
							0,
							DescriptorType::StorageBuffer,
							ArrayView<const DescriptorSet::BufferInfo>(emplacedBufferInfo)
						);
					}
					break;
#endif
					case PBRLightingPipeline::DescriptorBinding::Count:
						ExpectUnreachable();
				}
			}
		}
		else
		{
			// Raytraced
			[[maybe_unused]] auto emplaceInputAttachment = [this, &imageInfo, &descriptorUpdates, descriptorSet](
																											 const SampledTextures texture,
																											 const PBRLightingPipelineRaytraced::DescriptorBinding descriptorBinding,
																											 const ImageLayout imageLayout
																										 )
			{
				const uint8 index = (uint8)texture;
				Assert(m_imageMappingsViews[index].IsValid());

				DescriptorSet::ImageInfo& emplacedImageInfo =
					imageInfo.EmplaceBack(DescriptorSet::ImageInfo{m_samplers[index], m_imageMappingsViews[index], imageLayout});

				descriptorUpdates.EmplaceBack(
					descriptorSet,
					(uint8)descriptorBinding,
					0,
					DescriptorType::InputAttachment,
					ArrayView<const DescriptorSet::ImageInfo>(emplacedImageInfo)
				);
			};

			auto emplaceSampledTexture = [this, &imageInfo, &descriptorUpdates, descriptorSet](
																		 const SampledTextures texture,
																		 const PBRLightingPipelineRaytraced::DescriptorBinding descriptorBinding,
																		 const ImageLayout imageLayout
																	 )
			{
				const uint8 index = (uint8)texture;
				Assert(m_imageMappingsViews[index].IsValid());

				DescriptorSet::ImageInfo& emplacedImageInfo =
					imageInfo.EmplaceBack(DescriptorSet::ImageInfo{{}, m_imageMappingsViews[index], imageLayout});

				descriptorUpdates.EmplaceBack(
					descriptorSet,
					(uint8)descriptorBinding,
					0,
					DescriptorType::SampledImage,
					ArrayView<const DescriptorSet::ImageInfo>(emplacedImageInfo)
				);
			};

			auto emplaceSampler = [this, &imageInfo, &descriptorUpdates, descriptorSet](
															const SampledTextures texture,
															const PBRLightingPipelineRaytraced::DescriptorBinding descriptorBinding,
															const ImageLayout imageLayout
														)
			{
				const uint8 index = (uint8)texture;
				Assert(m_samplers[index].IsValid());

				DescriptorSet::ImageInfo& emplacedImageInfo = imageInfo.EmplaceBack(DescriptorSet::ImageInfo{m_samplers[index], {}, imageLayout});

				descriptorUpdates.EmplaceBack(
					descriptorSet,
					(uint8)descriptorBinding,
					0,
					DescriptorType::Sampler,
					ArrayView<const DescriptorSet::ImageInfo>(emplacedImageInfo)
				);
			};

			auto emplaceLightBuffer = [this, &bufferInfo, &descriptorUpdates, descriptorSet](
																	const LightTypes lightType,
																	const PBRLightingPipelineRaytraced::DescriptorBinding descriptorBinding,
																	const size structBaseSize,
																	const size structSize,
																	const uint32 maximumLightCount
																)
			{
				const FixedArrayView<const StorageBuffer, (uint8)LightTypes::Count - 1> lightStorageBuffers =
					m_tilePopulationStage.GetLightStorageBuffers();
				const StorageBuffer& lightBuffer = lightStorageBuffers[(uint8)lightType];
				const uint8 index = (uint8)lightType;
				const ArrayView<const ReferenceWrapper<const Entity::LightSourceComponent>> visibleLights =
					m_tilePopulationStage.GetVisibleLights((LightTypes)index);

				DescriptorSet::BufferInfo& emplacedBufferInfo = bufferInfo.EmplaceBack(DescriptorSet::BufferInfo{
					lightBuffer,
					0,
					structBaseSize + Math::Max(structSize * Math::Min(visibleLights.GetSize(), maximumLightCount), structSize)
				});

				descriptorUpdates.EmplaceBack(
					descriptorSet,
					(uint8)descriptorBinding,
					0,
					DescriptorType::StorageBuffer,
					ArrayView<const DescriptorSet::BufferInfo>(emplacedBufferInfo)
				);
			};

#if RENDERER_SUPPORTS_RAYTRACING
			// TODO: Wrap Vulkan acceleration structure update extension into our API
			AccelerationStructureView accelerationStructure;
#if RENDERER_VULKAN
			VkWriteDescriptorSetAccelerationStructureKHR descriptorAccelerationStructureInfo;
			VkAccelerationStructureKHR vkAccelerationStructure;
#endif
			if (m_pBuildAccelerationStructureStage.IsValid())
			{
				accelerationStructure = m_pBuildAccelerationStructureStage->GetInstancesAccelerationStructure();
#if RENDERER_VULKAN
				vkAccelerationStructure = accelerationStructure;
				descriptorAccelerationStructureInfo = VkWriteDescriptorSetAccelerationStructureKHR{
					VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
					nullptr,
					1,
					&vkAccelerationStructure
				};
#endif

				descriptorSet.BindIndirectAccelerationStructures(
					(uint8)PBRLightingPipelineRaytraced::DescriptorBinding::AccelerationStructure,
					m_pBuildAccelerationStructureStage->GetMeshAccelerationStructures()
				);
			}
#endif

			for (PBRLightingPipelineRaytraced::DescriptorBinding descriptorBinding = PBRLightingPipelineRaytraced::DescriptorBinding::First;
			     descriptorBinding < PBRLightingPipelineRaytraced::DescriptorBinding::Count;
			     descriptorBinding = PBRLightingPipelineRaytraced::DescriptorBinding((uint8)descriptorBinding + 1))
			{
				switch (descriptorBinding)
				{
					case PBRLightingPipelineRaytraced::DescriptorBinding::BRDFTexture:
						emplaceSampledTexture(SampledTextures::BRDF, descriptorBinding, ImageLayout::ShaderReadOnlyOptimal);
						break;
					case PBRLightingPipelineRaytraced::DescriptorBinding::BRDFSampler:
						emplaceSampler(SampledTextures::BRDF, descriptorBinding, ImageLayout::ShaderReadOnlyOptimal);
						break;
					case PBRLightingPipelineRaytraced::DescriptorBinding::TilesTexture:
						emplaceSampledTexture(SampledTextures::Clusters, descriptorBinding, ImageLayout::ShaderReadOnlyOptimal);
						break;
					case PBRLightingPipelineRaytraced::DescriptorBinding::TilesSampler:
						emplaceSampler(SampledTextures::Clusters, descriptorBinding, ImageLayout::ShaderReadOnlyOptimal);
						break;

#if ENABLE_DEFERRED_LIGHTING_SUBPASSES
					case PBRLightingPipelineRaytraced::DescriptorBinding::AlbedoTexture:
						emplaceInputAttachment(SampledTextures::Albedo, descriptorBinding, ImageLayout::ShaderReadOnlyOptimal);
						break;
					case PBRLightingPipelineRaytraced::DescriptorBinding::NormalsTexture:
						emplaceInputAttachment(SampledTextures::Normals, descriptorBinding, ImageLayout::ShaderReadOnlyOptimal);
						break;
					case PBRLightingPipelineRaytraced::DescriptorBinding::MaterialPropertiesTexture:
						emplaceInputAttachment(SampledTextures::MaterialProperties, descriptorBinding, ImageLayout::ShaderReadOnlyOptimal);
						break;
					case PBRLightingPipelineRaytraced::DescriptorBinding::DepthTexture:
						emplaceInputAttachment(SampledTextures::Depth, descriptorBinding, ImageLayout::DepthStencilReadOnlyOptimal);
						break;
#else
					case PBRLightingPipelineRaytraced::DescriptorBinding::AlbedoTexture:
						emplaceSampledTexture(SampledTextures::Albedo, descriptorBinding, ImageLayout::ShaderReadOnlyOptimal);
						break;
					case PBRLightingPipelineRaytraced::DescriptorBinding::AlbedoSampler:
						emplaceSampler(SampledTextures::Albedo, descriptorBinding, ImageLayout::ShaderReadOnlyOptimal);
						break;
					case PBRLightingPipelineRaytraced::DescriptorBinding::NormalsTexture:
						emplaceSampledTexture(SampledTextures::Normals, descriptorBinding, ImageLayout::ShaderReadOnlyOptimal);
						break;
					case PBRLightingPipelineRaytraced::DescriptorBinding::NormalsSampler:
						emplaceSampler(SampledTextures::Normals, descriptorBinding, ImageLayout::ShaderReadOnlyOptimal);
						break;
					case PBRLightingPipelineRaytraced::DescriptorBinding::MaterialPropertiesTexture:
						emplaceSampledTexture(SampledTextures::MaterialProperties, descriptorBinding, ImageLayout::ShaderReadOnlyOptimal);
						break;
					case PBRLightingPipelineRaytraced::DescriptorBinding::MaterialPropertiesSampler:
						emplaceSampler(SampledTextures::MaterialProperties, descriptorBinding, ImageLayout::ShaderReadOnlyOptimal);
						break;
					case PBRLightingPipelineRaytraced::DescriptorBinding::DepthTexture:
						emplaceSampledTexture(SampledTextures::Depth, descriptorBinding, ImageLayout::DepthStencilReadOnlyOptimal);
						break;
#endif

					case PBRLightingPipelineRaytraced::DescriptorBinding::IrradianceTexture:
						emplaceSampledTexture(SampledTextures::IrradianceArray, descriptorBinding, ImageLayout::ShaderReadOnlyOptimal);
						break;
					case PBRLightingPipelineRaytraced::DescriptorBinding::IrradianceSampler:
						emplaceSampler(SampledTextures::IrradianceArray, descriptorBinding, ImageLayout::ShaderReadOnlyOptimal);
						break;
					case PBRLightingPipelineRaytraced::DescriptorBinding::PrefilteredMapTexture:
						emplaceSampledTexture(SampledTextures::PrefilteredEnvironmentArray, descriptorBinding, ImageLayout::ShaderReadOnlyOptimal);
						break;
					case PBRLightingPipelineRaytraced::DescriptorBinding::PrefilteredMapSampler:
						emplaceSampler(SampledTextures::PrefilteredEnvironmentArray, descriptorBinding, ImageLayout::ShaderReadOnlyOptimal);
						break;
					case PBRLightingPipelineRaytraced::DescriptorBinding::AccelerationStructure:
					{
#if RENDERER_SUPPORTS_RAYTRACING
						if (accelerationStructure != nullptr)
						{
#if RENDERER_VULKAN
							DescriptorSet::UpdateInfo accelerationStructureUpdateInfo{
								descriptorSet,
								(uint8)descriptorBinding,
								0,
								DescriptorType::AccelerationStructure,
								ArrayView<const DescriptorSet::BufferInfo>{}
							};
							accelerationStructureUpdateInfo.m_count = 1;
							accelerationStructureUpdateInfo.m_pNext = &descriptorAccelerationStructureInfo;
#else
							DescriptorSet::UpdateInfo accelerationStructureUpdateInfo{descriptorSet, (uint8)descriptorBinding, accelerationStructure};
#endif

							descriptorUpdates.EmplaceBack(accelerationStructureUpdateInfo);
						}
#endif
					}
					break;
					case PBRLightingPipelineRaytraced::DescriptorBinding::PointLightsBuffer:
						emplaceLightBuffer(
							LightTypes::PointLight,
							descriptorBinding,
							LightHeaderSizes[(uint8)LightTypes::PointLight],
							LightStructSizes[(uint8)LightTypes::PointLight],
							(uint32)MaximumLightCounts[(uint8)LightTypes::PointLight]
						);
						break;
					case PBRLightingPipelineRaytraced::DescriptorBinding::SpotLightsBuffer:
						emplaceLightBuffer(
							LightTypes::SpotLight,
							descriptorBinding,
							LightHeaderSizes[(uint8)LightTypes::SpotLight],
							LightStructSizes[(uint8)LightTypes::SpotLight],
							(uint32)MaximumLightCounts[(uint8)LightTypes::SpotLight]
						);
						break;
					case PBRLightingPipelineRaytraced::DescriptorBinding::DirectionalLightsBuffer:
						emplaceLightBuffer(
							LightTypes::DirectionalLight,
							descriptorBinding,
							LightHeaderSizes[(uint8)LightTypes::DirectionalLight],
							LightStructSizes[(uint8)LightTypes::DirectionalLight],
							(uint32)MaximumLightCounts[(uint8)LightTypes::DirectionalLight]
						);
						break;
					case PBRLightingPipelineRaytraced::DescriptorBinding::RenderItemsBuffer:
					{
						Rendering::StageCache& stageCache = System::Get<Renderer>().GetStageCache();

						const SceneRenderStageIdentifier materialsStageIdentifier = stageCache.FindIdentifier(MaterialsStage::TypeGuid);
						const MaterialsStage& materialsStage = static_cast<MaterialsStage&>(*m_sceneView.GetRenderItemStage(materialsStageIdentifier));

						const StorageBuffer& renderItemsDataBuffer = materialsStage.GetRenderItemsDataBuffer();
						DescriptorSet::BufferInfo& emplacedBufferInfo =
							bufferInfo.EmplaceBack(DescriptorSet::BufferInfo{renderItemsDataBuffer, 0, renderItemsDataBuffer.GetSize()});

						descriptorUpdates.EmplaceBack(
							descriptorSet,
							(uint8)descriptorBinding,
							0,
							DescriptorType::StorageBuffer,
							ArrayView<const DescriptorSet::BufferInfo>(emplacedBufferInfo)
						);
					}
					break;
					case PBRLightingPipelineRaytraced::DescriptorBinding::Count:
						ExpectUnreachable();
				}
			}
		}

		DescriptorSet::Update(m_sceneView.GetLogicalDevice(), descriptorUpdates);
	}

	void PBRLightingStage::OnRenderItemsBecomeVisible(const Entity::RenderItemMask& renderItems)
	{
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

			Assert(pVisibleComponent->IsLight());
			const Entity::LightSourceComponent& lightComponent = pVisibleComponent->AsExpected<Entity::LightSourceComponent>();

			switch (LightGatheringStage::GetLightType(lightComponent))
			{
				case LightTypes::EnvironmentLight:
				{
					OptionalIterator<Entity::RenderItemIdentifier> pAvailableSlot =
						m_visibleEnvironmentLights.GetView().Find(Entity::RenderItemIdentifier{});
					Assert(pAvailableSlot.IsValid());
					if (pAvailableSlot.IsValid())
					{
						*pAvailableSlot = lightComponent.GetRenderItemIdentifier();

						const Entity::EnvironmentLightComponent& environmentLight = static_cast<const Entity::EnvironmentLightComponent&>(lightComponent
						);
						Threading::UniqueLock lock(m_textureLoadMutex);
						if (m_imageMappingsViews[(uint8)SampledTextures::IrradianceArray].IsValid() && m_imageMappingsViews[(uint8)SampledTextures::PrefilteredEnvironmentArray].IsValid())
						{
							LoadEnvironmentLightTextures(environmentLight, m_visibleEnvironmentLights.GetIteratorIndex(pAvailableSlot));
						}
					}
				}
				break;
				default:
					break;
			}
		}

		if (m_descriptorSet.IsValid())
		{
			UpdateLightBufferDescriptorSet();
		}
		else
		{
			m_shouldUpdateLightBuffer = true;
		}
	}

	void PBRLightingStage::OnVisibleRenderItemsReset(const Entity::RenderItemMask& renderItems)
	{
		// TODO: Resetting
		OnRenderItemsBecomeHidden(renderItems);

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

		OnRenderItemsBecomeVisible(renderItems);
	}

	void PBRLightingStage::OnRenderItemsBecomeHidden(const Entity::RenderItemMask& renderItems)
	{
		const typename Entity::RenderItemIdentifier::IndexType maximumUsedRenderItemCount =
			m_sceneView.GetSceneChecked()->GetMaximumUsedRenderItemCount();
		for (const uint32 renderItemIndex : renderItems.GetSetBitsIterator(0, maximumUsedRenderItemCount))
		{
			const Entity::RenderItemIdentifier renderItemIdentifier = Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex);
			if (OptionalIterator<Entity::RenderItemIdentifier> pAvailableSlot = m_visibleEnvironmentLights.GetView().Find(renderItemIdentifier))
			{
				*pAvailableSlot = Entity::RenderItemIdentifier{};
			}
		}

		if (m_descriptorSet.IsValid())
		{
			UpdateLightBufferDescriptorSet();
		}
		else
		{
			m_shouldUpdateLightBuffer = true;
		}
	}

	void PBRLightingStage::OnVisibleRenderItemTransformsChanged(const Entity::RenderItemMask&)
	{
		if (m_descriptorSet.IsValid())
		{
			UpdateLightBufferDescriptorSet();
		}
		else
		{
			m_shouldUpdateLightBuffer = true;
		}
	}

	Threading::JobBatch PBRLightingStage::LoadRenderItemsResources(const Entity::RenderItemMask&)
	{
		Threading::JobBatch jobBatch;
		Assert(false, "TODO: Load environment map");
		/*const typename Entity::RenderItemIdentifier::IndexType maximumUsedRenderItemCount =
		  m_sceneView.GetSceneChecked()->GetMaximumUsedRenderItemCount();
		for (const uint32 renderItemIndex : renderItems.GetSetBitsIterator(0, maximumUsedRenderItemCount))
		{
		  const Entity::RenderItemIdentifier renderItemIdentifier = Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex);
		  const Entity::RenderItemComponent& renderItem = *m_sceneView.GetVisibleRenderItemComponent(renderItemIdentifier);

		  Assert(renderItem.IsLight());
		  const Entity::LightSourceComponent& lightComponent = static_cast<const Entity::LightSourceComponent&>(renderItem);

		  switch (LightGatheringStage::GetLightType(lightComponent))
		  {
		    case LightTypes::EnvironmentLight:
		    {
		      OptionalIterator<Entity::RenderItemIdentifier> pAvailableSlot =
		        m_visibleEnvironmentLights.GetView().Find(Entity::RenderItemIdentifier{});
		      Assert(pAvailableSlot.IsValid());
		      if (pAvailableSlot.IsValid())
		      {
		        *pAvailableSlot = lightComponent.GetRenderItemIdentifier();

		        const Entity::EnvironmentLightComponent& environmentLight = static_cast<const Entity::EnvironmentLightComponent&>(lightComponent
		        );
		        if (m_loadedAllTextures)
		        {
		          LoadEnvironmentLightTextures(environmentLight, m_visibleEnvironmentLights.GetIteratorIndex(pAvailableSlot));
		        }
		        else
		        {
		          Threading::UniqueLock lock(m_textureLoadMutex);
		          if (m_loadedAllTextures)
		          {
		            LoadEnvironmentLightTextures(environmentLight, m_visibleEnvironmentLights.GetIteratorIndex(pAvailableSlot));
		          }
		        }
		      }
		    }
		    break;
		    default:
		      break;
		  }
		}*/
		return jobBatch;
	}

	void
	PBRLightingStage::LoadEnvironmentLightTextures(const Entity::EnvironmentLightComponent& environmentLight, const uint8 arrayLayerIndex)
	{
		Threading::JobBatch jobBatch = LoadEnvironmentLightTextures(
			environmentLight.GetIrradianceTexture(),
			environmentLight.GetPrefilteredEnvironmentTexture(),
			arrayLayerIndex
		);
		if (jobBatch.IsValid())
		{
			Threading::JobRunnerThread::GetCurrent()->Queue(jobBatch);
		}
	}

	[[nodiscard]] Threading::JobBatch PBRLightingStage::LoadEnvironmentLightTextures(
		const TextureIdentifier irradianceTextureIdentifier,
		TextureIdentifier prefilteredEnvironmentTextureIdentifier,
		const uint8 arrayLayerIndex
	)
	{
		TextureCache& textureCache = System::Get<Rendering::Renderer>().GetTextureCache();
		RenderTargetCache& renderTargetCache = m_sceneView.GetOutput().GetRenderTargetCache();

		auto copyTexture = [&textureCache, &renderTargetCache, this, arrayLayerIndex](
												 const uint8 sampledTextureIndex,
												 const MipMask mipMask,
												 const Rendering::TextureIdentifier sourceTexture
											 ) -> Optional<Threading::Job*>
		{
			return textureCache.GetOrLoadRenderTexture(
				m_sceneView.GetLogicalDevice().GetIdentifier(),
				sourceTexture,
				ImageMappingType::Cube,
				mipMask,
				TextureLoadFlags::Default & ~TextureLoadFlags::LoadDummy,
				TextureCache::TextureLoadListenerData(
					*this,
					[this,
			     &textureCache,
			     &renderTargetCache,
			     sampledTextureIndex,
			     arrayLayerIndex](PBRLightingStage&, const LogicalDevice& logicalDevice, const TextureIdentifier, RenderTexture& sourceTexture, const MipMask sourceChangedMips, const EnumFlags<LoadedTextureFlags>)
						-> EventCallbackResult
					{
						Optional<Rendering::RenderTexture*> pTargetTexture = textureCache.GetRenderTexture(
							m_sceneView.GetLogicalDevice().GetIdentifier(),
							renderTargetCache.FindOrRegisterRenderTargetFromTemplateIdentifier(
								textureCache,
								textureCache.FindOrRegisterRenderTargetTemplate(StaticRenderTargets[sampledTextureIndex])
							)
						);

						if (pTargetTexture.IsInvalid())
						{
							return EventCallbackResult::Keep;
						}

						Assert(sourceTexture.GetFormat() == pTargetTexture->GetFormat());
						if (LIKELY(sourceTexture.GetFormat() == pTargetTexture->GetFormat()))
						{
							const MipMask targetMipMask = pTargetTexture->GetTotalMipMask();
							const uint32 firstTargetMipLevel = *targetMipMask.GetFirstIndex();
							[[maybe_unused]] const uint32 targetMipCount = targetMipMask.GetSize();
							const MipMask sourceMipMask = sourceTexture.GetTotalMipMask();
							const uint32 firstSourceMipLevel = *sourceMipMask.GetFirstIndex();
							[[maybe_unused]] const uint32 sourceMipCount = sourceMipMask.GetSize();
							Assert((sourceMipMask & sourceChangedMips).AreAnySet());
							const MipMask pendingMipMask = sourceChangedMips & targetMipMask;
							const uint32 changedMipCount = pendingMipMask.GetSize();

							InlineVector<ImageCopy, 24> copiedRegions(Memory::Reserve, changedMipCount * sourceTexture.GetTotalArrayCount());

							const uint32 faceCount = sourceTexture.GetTotalArrayCount();

							MipMask changedSourceMipMask, changedTargetMipMask;

							for (MipMask::StoredType changedSourceMipLevel : Memory::GetSetBitsIterator(pendingMipMask.GetValue()))
							{
								const uint32 mipExtent = 1 << changedSourceMipLevel;

								const MipMask::StoredType relativeSourceMipLevel = MipMask::StoredType(changedSourceMipLevel - firstSourceMipLevel);
								const MipMask::StoredType relativeTargetMipLevel = MipMask::StoredType(changedSourceMipLevel - firstTargetMipLevel);
								const MipMask::StoredType sourceMipLevel = MipMask::StoredType(sourceMipCount - relativeSourceMipLevel - 1);
								const MipMask::StoredType targetMipLevel = MipMask::StoredType(targetMipCount - relativeTargetMipLevel - 1);
								copiedRegions.EmplaceBack(ImageCopy{
									SubresourceLayers{ImageAspectFlags::Color, sourceMipLevel, ArrayRange{0, faceCount}},
									Math::Vector3i{Math::Zero},
									SubresourceLayers{ImageAspectFlags::Color, targetMipLevel, ArrayRange{uint32(faceCount * arrayLayerIndex), faceCount}},
									Math::Vector3i{Math::Zero},
									Math::Vector3ui{mipExtent, mipExtent, 1}
								});

								changedSourceMipMask |= MipMask::FromIndex(MipMask::StoredType(relativeSourceMipLevel));
								changedTargetMipMask |= MipMask::FromIndex(MipMask::StoredType(relativeTargetMipLevel));
							}

							const MipRange sourceMipRange = changedSourceMipMask.GetRange(sourceTexture.GetTotalMipCount());
							const MipRange targetMipRange = changedTargetMipMask.GetRange(pTargetTexture->GetTotalMipCount());

							if (copiedRegions.HasElements())
							{
								Threading::EngineJobRunnerThread& thread = *Threading::EngineJobRunnerThread::GetCurrent();

								SingleUseCommandBuffer singleUseCommandBuffer(
									m_sceneView.GetLogicalDevice(),
									thread.GetRenderData().GetCommandPool(logicalDevice.GetIdentifier(), Rendering::QueueFamily::Graphics),
									thread,
									Rendering::QueueFamily::Graphics,
									Threading::JobPriority::CoreRenderStageResources
								);

								singleUseCommandBuffer.OnFinished = [this]()
								{
									m_shouldUpdateLightBuffer = true;
								};

								const Rendering::CommandEncoderView commandEncoder = singleUseCommandBuffer;

								{
									BarrierCommandEncoder barrierCommandEncoder = commandEncoder.BeginBarrier();

									barrierCommandEncoder.TransitionImageLayout(
										Rendering::PipelineStageFlags::Transfer,
										Rendering::AccessFlags::TransferWrite,
										Rendering::ImageLayout::TransferDestinationOptimal,
										*pTargetTexture,
										Rendering::ImageSubresourceRange{
											Rendering::ImageAspectFlags::Color,
											targetMipRange,
											ArrayRange{0, pTargetTexture->GetTotalArrayCount()}
										}
									);
									barrierCommandEncoder.TransitionImageLayout(
										Rendering::PipelineStageFlags::Transfer,
										AccessFlags::TransferRead,
										ImageLayout::TransferSourceOptimal,
										sourceTexture,
										Rendering::ImageSubresourceRange{
											ImageAspectFlags::Color,
											sourceMipRange,
											ArrayRange{0, sourceTexture.GetTotalArrayCount()}
										}
									);
								}

								{
									BlitCommandEncoder blitCommandEncoder = commandEncoder.BeginBlit();
									blitCommandEncoder.RecordCopyImageToImage(
										sourceTexture,
										ImageLayout::TransferSourceOptimal,
										*pTargetTexture,
										ImageLayout::TransferDestinationOptimal,
										copiedRegions.GetView()
									);
								}

								{
									BarrierCommandEncoder barrierCommandEncoder = commandEncoder.BeginBarrier();

									barrierCommandEncoder.TransitionImageLayout(
										Rendering::PipelineStageFlags::FragmentShader,
										Rendering::AccessFlags::ShaderRead,
										Rendering::ImageLayout::ShaderReadOnlyOptimal,
										*pTargetTexture,
										Rendering::ImageSubresourceRange{
											Rendering::ImageAspectFlags::Color,
											targetMipRange,
											ArrayRange{0, pTargetTexture->GetTotalArrayCount()}
										}
									);
									barrierCommandEncoder.TransitionImageLayout(
										Rendering::PipelineStageFlags::FragmentShader,
										AccessFlags::ShaderRead,
										ImageLayout::ShaderReadOnlyOptimal,
										sourceTexture,
										Rendering::ImageSubresourceRange{
											ImageAspectFlags::Color,
											sourceMipRange,
											ArrayRange{0, sourceTexture.GetTotalArrayCount()}
										}
									);
								}
							}
						}
						return EventCallbackResult::Keep;
					}
				)
			);
		};

		Threading::JobBatch jobBatch;
		Assert(m_loadedIrradianceTextures[arrayLayerIndex].IsInvalid(), "Must deregister texture listener!");
		m_loadedIrradianceTextures[arrayLayerIndex] = irradianceTextureIdentifier;
		if (const Optional<Threading::Job*> pLoadIrradianceTextureJob = copyTexture((uint8)SampledTextures::IrradianceArray, StaticRenderTargetMipMasks[(uint8)SampledTextures::IrradianceArray], irradianceTextureIdentifier))
		{
			jobBatch.QueueAfterStartStage(*pLoadIrradianceTextureJob);
		}

		Assert(m_loadedPrefilteredTextures[arrayLayerIndex].IsInvalid(), "Must deregister texture listener!");
		m_loadedPrefilteredTextures[arrayLayerIndex] = prefilteredEnvironmentTextureIdentifier;
		if (const Optional<Threading::Job*> pPrefilteredEnvironmentTextureJob = copyTexture((uint8)SampledTextures::PrefilteredEnvironmentArray, StaticRenderTargetMipMasks[(uint8)SampledTextures::PrefilteredEnvironmentArray], prefilteredEnvironmentTextureIdentifier))
		{
			jobBatch.QueueAfterStartStage(*pPrefilteredEnvironmentTextureJob);
		}
		return jobBatch;
	}

	void PBRLightingStage::OnSceneUnloaded()
	{
		m_visibleEnvironmentLights.GetView().ZeroInitialize();
		m_loadedIrradianceTextures.GetView().ZeroInitialize();
		m_loadedPrefilteredTextures.GetView().ZeroInitialize();

		TextureCache& textureCache = System::Get<Rendering::Renderer>().GetTextureCache();
		for (Rendering::TextureIdentifier textureIdentifier : m_loadedIrradianceTextures)
		{
			if (textureIdentifier.IsValid())
			{
				textureCache.RemoveRenderTextureListener(m_sceneView.GetLogicalDevice().GetIdentifier(), textureIdentifier, this);
			}
		}
		for (Rendering::TextureIdentifier textureIdentifier : m_loadedPrefilteredTextures)
		{
			if (textureIdentifier.IsValid())
			{
				textureCache.RemoveRenderTextureListener(m_sceneView.GetLogicalDevice().GetIdentifier(), textureIdentifier, this);
			}
		}
	}

	void PBRLightingStage::OnActiveCameraPropertiesChanged(const Rendering::CommandEncoderView, PerFrameStagingBuffer&)
	{
		if (m_descriptorSet.IsValid())
		{
			UpdateLightBufferDescriptorSet();
		}
		else
		{
			m_shouldUpdateLightBuffer = true;
		}
	}

	void PBRLightingStage::UpdateLightBufferDescriptorSet()
	{
		Assert(m_descriptorSet.IsValid());

		// One more for SDSMFinalLightBuffer
		FlatVector<DescriptorSet::UpdateInfo, (uint8)LightTypes::RealLightCount + ENABLE_SAMPLE_DISTRIBUTION_SHADOW_MAPS> descriptorUpdates;
		FlatVector<DescriptorSet::BufferInfo, (uint8)LightTypes::RealLightCount + ENABLE_SAMPLE_DISTRIBUTION_SHADOW_MAPS> bufferInfo;

		for (LightTypes lightType = LightTypes::First; lightType <= LightTypes::LastRealLight; lightType = LightTypes((uint8)lightType + 1))
		{
			bufferInfo.EmplaceBack(DescriptorSet::BufferInfo{
				m_tilePopulationStage.GetLightStorageBuffer(lightType),
				0,
				Math::Max(m_tilePopulationStage.GetLightStorageBufferSize(lightType), LightStructSizes[(uint8)lightType])
			});

			auto getDescriptorBinding = [](const LightTypes lightType)
			{
				switch (lightType)
				{
					case LightTypes::PointLight:
						return PBRLightingPipeline::DescriptorBinding::PointLightsBuffer;
					case LightTypes::SpotLight:
						return PBRLightingPipeline::DescriptorBinding::SpotLightsBuffer;
					case LightTypes::DirectionalLight:
						return PBRLightingPipeline::DescriptorBinding::DirectionalLightsBuffer;
					case LightTypes::EnvironmentLight:
					case LightTypes::Count:
						ExpectUnreachable();
				}
				ExpectUnreachable();
			};

			descriptorUpdates.EmplaceBack(DescriptorSet::UpdateInfo{
				m_descriptorSet,
				(uint8)getDescriptorBinding(lightType),
				0,
				DescriptorType::StorageBuffer,
				ArrayView<const DescriptorSet::BufferInfo>(bufferInfo.GetLastElement())
			});
		}

		if (m_pShadowsStage.IsInvalid() || m_pShadowsStage->GetState() == ShadowsStage::State::Rasterized)
		{
			Assert(m_pShadowsStage->GetSDSMShadowSamplingInfoBuffer().IsValid());
			bufferInfo.EmplaceBack(DescriptorSet::BufferInfo{
				m_pShadowsStage->GetSDSMShadowSamplingInfoBuffer(),
				0,
				m_pShadowsStage->GetSDSMShadowSamplingInfoBufferSize()
			});
			descriptorUpdates.EmplaceBack(DescriptorSet::UpdateInfo{
				m_descriptorSet,
				(uint8)PBRLightingPipeline::DescriptorBinding::ShadowInfoBuffer,
				0,
				DescriptorType::StorageBuffer,
				ArrayView<const DescriptorSet::BufferInfo>(bufferInfo.GetLastElement())
			});
		}

		Threading::UniqueLock lock(m_textureLoadMutex);
		DescriptorSet::Update(m_sceneView.GetLogicalDevice(), descriptorUpdates);
	}

	void PBRLightingStage::OnBeforeRenderPassDestroyed()
	{
		m_rasterizedPipeline.PrepareForResize(m_sceneView.GetLogicalDevice());
		m_raytracedPipeline.PrepareForResize(m_sceneView.GetLogicalDevice());

		m_loadedAllTextures = false;

		if (m_pShadowsStage.IsInvalid() || m_pShadowsStage->GetState() == ShadowsStage::State::Rasterized)
		{
			m_loadingTexturesMask |= (SampledTexturesMaskType)SampledTextures::RasterizedAttachmentsMask;
		}
		else
		{
			m_loadingTexturesMask |= (SampledTexturesMaskType)SampledTextures::RaytracedAttachmentsMask;
		}

		Renderer& renderer = System::Get<Renderer>();
		TextureCache& textureCache = renderer.GetTextureCache();
		for (Rendering::TextureIdentifier textureIdentifier : m_loadedIrradianceTextures)
		{
			if (textureIdentifier.IsValid())
			{
				textureCache.RemoveRenderTextureListener(m_sceneView.GetLogicalDevice().GetIdentifier(), textureIdentifier, this);
			}
		}

		for (Rendering::TextureIdentifier textureIdentifier : m_loadedPrefilteredTextures)
		{
			if (textureIdentifier.IsValid())
			{
				textureCache.RemoveRenderTextureListener(m_sceneView.GetLogicalDevice().GetIdentifier(), textureIdentifier, this);
			}
		}
	}

	Threading::JobBatch PBRLightingStage::AssignRenderPass(
		const RenderPassView renderPass, const Math::Rectangleui outputArea, const Math::Rectangleui fullRenderArea, const uint8 subpassIndex
	)
	{
		m_shouldUpdateLightBuffer = true;
		if (m_pShadowsStage.IsInvalid() || m_pShadowsStage->GetState() == ShadowsStage::State::Rasterized)
		{
			return m_rasterizedPipeline.CreatePipeline(
				m_sceneView.GetLogicalDevice(),
				m_sceneView.GetLogicalDevice().GetShaderCache(),
				renderPass,
				outputArea,
				fullRenderArea,
				subpassIndex
			);
		}
		else
		{
			// Raytraced
			return m_raytracedPipeline.CreatePipeline(
				m_sceneView.GetLogicalDevice(),
				m_sceneView.GetLogicalDevice().GetShaderCache(),
				renderPass,
				outputArea,
				fullRenderArea,
				subpassIndex
			);
		}
	}

	void PBRLightingStage::OnRenderPassAttachmentsLoaded(
		[[maybe_unused]] const Math::Vector2ui resolution,
		[[maybe_unused]] const ArrayView<ArrayView<const ImageMappingView, uint16>, Rendering::FrameIndex> colorAttachmentMappings,
		[[maybe_unused]] const ArrayView<ImageMappingView, Rendering::FrameIndex> depthAttachmentMapping,
		[[maybe_unused]] const ArrayView<ArrayView<const ImageMappingView, uint16>, Rendering::FrameIndex> subpassInputAttachmentMappings,
		[[maybe_unused]] const ArrayView<ArrayView<const ImageMappingView, uint16>, Rendering::FrameIndex> externalInputAttachmentMappings,
		[[maybe_unused]] const ArrayView<const Math::Vector2ui, uint16> externalInputAttachmentResolutions,
		[[maybe_unused]] const ArrayView<ArrayView<const Optional<RenderTexture*>, uint16>, Rendering::FrameIndex> colorAttachments,
		[[maybe_unused]] const uint8 subpassIndex
	)
	{
		Assert(subpassIndex == 1);

		if (m_pShadowsStage.IsInvalid() || m_pShadowsStage->GetState() == ShadowsStage::State::Rasterized)
		{
			m_imageMappingsViews[(uint8)SampledTextures::Albedo] =
				subpassInputAttachmentMappings[0]
																			[(uint8)Rasterized::Attachment::Albedo - (uint8)Rasterized::Attachment::SubpassInputAttachmentsBegin];
			m_imageMappingsViews[(uint8)SampledTextures::Normals] = subpassInputAttachmentMappings
				[0][(uint8)Rasterized::Attachment::Normals - (uint8)Rasterized::Attachment::SubpassInputAttachmentsBegin];
			m_imageMappingsViews[(uint8)SampledTextures::MaterialProperties] = subpassInputAttachmentMappings
				[0][(uint8)Rasterized::Attachment::MaterialProperties - (uint8)Rasterized::Attachment::SubpassInputAttachmentsBegin];
			m_imageMappingsViews[(uint8)SampledTextures::Depth] =
				subpassInputAttachmentMappings[0]
																			[(uint8)Rasterized::Attachment::Depth - (uint8)Rasterized::Attachment::SubpassInputAttachmentsBegin];

			m_imageMappingsViews[(uint8)SampledTextures::Clusters] = externalInputAttachmentMappings
				[0][(uint8)Rasterized::Attachment::Clusters - (uint8)Rasterized::Attachment::ExternalInputAttachmentsBegin];
			m_imageMappingsViews[(uint8)SampledTextures::ShadowmapArray] = externalInputAttachmentMappings
				[0][(uint8)Rasterized::Attachment::ShadowmapArray - (uint8)Rasterized::Attachment::ExternalInputAttachmentsBegin];

			const SampledTexturesMaskType previousLoadingTexturesMask = m_loadingTexturesMask.FetchAnd((SampledTexturesMaskType) ~(uint16
			)SampledTextures::RasterizedAttachmentsMask);
			Assert(
				(previousLoadingTexturesMask & (SampledTexturesMaskType)SampledTextures::RasterizedAttachmentsMask) ==
				(SampledTexturesMaskType)SampledTextures::RasterizedAttachmentsMask
			);
			if (previousLoadingTexturesMask == (SampledTexturesMaskType)SampledTextures::RasterizedAttachmentsMask)
			{
				OnTexturesLoaded();
			}
		}
		else
		{
			m_imageMappingsViews[(uint8)SampledTextures::Albedo] =
				subpassInputAttachmentMappings[0]
																			[(uint8)Raytraced::Attachment::Albedo - (uint8)Raytraced::Attachment::SubpassInputAttachmentsBegin];
			m_imageMappingsViews[(uint8)SampledTextures::Normals] =
				subpassInputAttachmentMappings[0]
																			[(uint8)Raytraced::Attachment::Normals - (uint8)Raytraced::Attachment::SubpassInputAttachmentsBegin];
			m_imageMappingsViews[(uint8)SampledTextures::MaterialProperties] = subpassInputAttachmentMappings
				[0][(uint8)Raytraced::Attachment::MaterialProperties - (uint8)Raytraced::Attachment::SubpassInputAttachmentsBegin];
			m_imageMappingsViews[(uint8)SampledTextures::Depth] =
				subpassInputAttachmentMappings[0][(uint8)Raytraced::Attachment::Depth - (uint8)Raytraced::Attachment::SubpassInputAttachmentsBegin];

			m_imageMappingsViews[(uint8)SampledTextures::Clusters] = externalInputAttachmentMappings
				[0][(uint8)Raytraced::Attachment::Clusters - (uint8)Raytraced::Attachment::ExternalInputAttachmentsBegin];

			const SampledTexturesMaskType previousLoadingTexturesMask = m_loadingTexturesMask.FetchAnd((SampledTexturesMaskType) ~(uint16
			)SampledTextures::RaytracedAttachmentsMask);
			Assert(
				(previousLoadingTexturesMask & (SampledTexturesMaskType)SampledTextures::RaytracedAttachmentsMask) ==
				(SampledTexturesMaskType)SampledTextures::RaytracedAttachmentsMask
			);
			if (previousLoadingTexturesMask == (SampledTexturesMaskType)SampledTextures::RaytracedAttachmentsMask)
			{
				OnTexturesLoaded();
			}
		}
	}

	bool PBRLightingStage::ShouldRecordCommands() const
	{
		const bool areBaseResourcesLoaded = m_loadedAllTextures & m_descriptorSet.IsValid() & m_sceneView.HasActiveCamera();
		if (!areBaseResourcesLoaded)
		{
			return false;
		}

		if (m_pShadowsStage.IsInvalid() || m_pShadowsStage->GetState() == ShadowsStage::State::Rasterized)
		{
			if (!m_rasterizedPipeline.IsValid())
			{
				return false;
			}
		}
		else
		{
			if (!m_raytracedPipeline.IsValid())
			{
				return false;
			}
		}

		return true;
	}

	void PBRLightingStage::OnBeforeRecordCommands(const CommandEncoderView)
	{
		if (WasSkipped())
		{
			return;
		}

#if RENDERER_SUPPORTS_RAYTRACING
		if (m_pShadowsStage.IsValid() && m_pShadowsStage->GetState() == ShadowsStage::State::Raytraced)
		{
			if (!m_pBuildAccelerationStructureStage->GetInstancesAccelerationStructure().IsValid())
			{
				return;
			}
		}
#endif

		PopulateDescriptorSet(m_descriptorSet);

		bool expected = true;
		if (m_descriptorSet.IsValid() && m_shouldUpdateLightBuffer.CompareExchangeStrong(expected, false))
		{
			UpdateLightBufferDescriptorSet();
		}
	}

	void PBRLightingStage::RecordRenderPassCommands(
		RenderCommandEncoder& renderCommandEncoder,
		const ViewMatrices& viewMatrices,
		[[maybe_unused]] const Math::Rectangleui renderArea,
		[[maybe_unused]] const uint8 subpassIndex
	)
	{
#if RENDERER_SUPPORTS_RAYTRACING
		if (m_pShadowsStage.IsValid() && m_pShadowsStage->GetState() == ShadowsStage::State::Raytraced)
		{
			if (!m_pBuildAccelerationStructureStage->GetInstancesAccelerationStructure().IsValid())
			{
				return;
			}
		}
#endif

		Renderer& renderer = System::Get<Renderer>();
		TextureCache& textureCache = renderer.GetTextureCache();
		MeshCache& meshCache = renderer.GetMeshCache();

		FlatVector<DescriptorSetView, 4> descriptorSets{m_descriptorSet.AtomicLoad(), viewMatrices.GetDescriptorSet()};
		{
			const DescriptorSetView texturesDescriptorSet = textureCache.GetTexturesDescriptorSet(m_logicalDevice.GetIdentifier());
			if (texturesDescriptorSet.IsValid())
			{
				descriptorSets.EmplaceBack(texturesDescriptorSet);
			}

			const DescriptorSetView meshesDescriptorSet = meshCache.GetMeshesDescriptorSet(m_logicalDevice.GetIdentifier());
			if (meshesDescriptorSet.IsValid())
			{
				descriptorSets.EmplaceBack(meshesDescriptorSet);
			}
		}

		if (m_pShadowsStage.IsInvalid() || m_pShadowsStage->GetState() == ShadowsStage::State::Rasterized)
		{
			renderCommandEncoder.BindPipeline(m_rasterizedPipeline);
			renderCommandEncoder.BindDescriptorSets(m_rasterizedPipeline, descriptorSets, m_rasterizedPipeline.GetFirstDescriptorSetIndex());
		}
		else
		{
			renderCommandEncoder.BindPipeline(m_raytracedPipeline);
			renderCommandEncoder.BindDescriptorSets(m_raytracedPipeline, descriptorSets, m_raytracedPipeline.GetFirstDescriptorSetIndex());
		}

		renderCommandEncoder.Draw(6, 1);
	}
}
