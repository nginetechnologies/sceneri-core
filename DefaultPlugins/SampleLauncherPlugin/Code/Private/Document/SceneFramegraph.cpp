#include <SamplerLauncher/Document/SceneFramegraph.h>

#include <Renderer/Renderer.h>
#include <Renderer/Devices/LogicalDevice.h>
#include <Renderer/Stages/MaterialStage.h>
#include <Renderer/Stages/MaterialsStage.h>
#include <Renderer/Stages/LateStageVisibilityCheckStage.h>
#include <Renderer/Stages/Pass.h>
#include <Renderer/RenderOutput/RenderOutput.h>
#include <Renderer/Scene/SceneView.h>
#include <Renderer/Scene/SceneViewDrawer.h>
#include <Renderer/Wrappers/SubpassDependency.h>
#include <Renderer/Framegraph/FramegraphBuilder.h>

#include <DeferredShading/MaterialIdentifiersStage.h>
#include <DeferredShading/PBRLightingStage.h>
#include <DeferredShading/DepthMinMaxPyramidStage.h>
#include <DeferredShading/ShadowsStage.h>
#include <DeferredShading/TilePopulationStage.h>
#include <DeferredShading/BuildAccelerationStructureStage.h>
#include <DeferredShading/SSAOStage.h>
#include <DeferredShading/SSRStage.h>
#include <DeferredShading/PostProcessStage.h>

#include <Widgets/Stages/WidgetCopyToScreenStage.h>

#include <FontRendering/Stages/DrawTextStage.h>

#include <Engine/Asset/AssetManager.h>

#include <Common/System/Query.h>
#include <Common/Threading/Jobs/JobRunnerThread.h>

namespace ngine::App::UI
{
	using namespace Rendering;

	SceneFramegraphBuilder::SceneFramegraphBuilder() = default;
	SceneFramegraphBuilder::~SceneFramegraphBuilder() = default;

	Optional<Rendering::MaterialStage*> SceneFramegraphBuilder::EmplaceMaterialStage(
		Rendering::SceneView& sceneView, const Asset::Guid guid, const float renderAreaFactor, const EnumFlags<Stage::Flags> stageFlags
	)
	{
		if (System::Get<Asset::Manager>().HasAsset(guid))
		{
			const Rendering::MaterialIdentifier materialIdentifier =
				System::Get<Rendering::Renderer>().GetMaterialCache().FindOrRegisterAsset(guid);
			return *m_materialStages.EmplaceBack(
				UniquePtr<Rendering::MaterialStage>::Make(materialIdentifier, sceneView, renderAreaFactor, stageFlags)
			);
		}

		return nullptr;
	}

	inline static constexpr Array DeferredMaterialAssetGuids{
		// Textured deferred PBR material
		"ADE8B81E-1EDF-443A-8AF7-37473EDF1390"_asset,

		// Textured deferred PBR + bloom material
		"a342b17f-be9f-4610-b973-0bc1f4157b5b"_asset,

		// Two-sided Textured deferred PBR material
		"2D8C6126-3C52-4463-BA1A-1DA8E4AC3666"_asset,

		// Masked textured deferred PBR material
		"B29170E0-BD18-4F96-8FED-8CB8A27C1C58"_asset,

		// Two-sided masked textured deferred PBR material
		"7942EB3F-D202-4030-A7BE-03F4D886F43D"_asset,

		// Textured constant deferred PBR material
		"846A87E3-B738-4471-A336-BCAC78E126B1"_asset,

		// Two-sided Textured constant deferred PBR material
		"5F63E2E7-0719-45BA-AE23-BA592CE87CD9"_asset,

		// Masked textured constant deferred PBR material
		"{44E1EC4C-00F6-4C5E-9B71-C7B7C510DF71}"_asset,

		// Masked two-sided Textured constant deferred PBR material
		"{AC0BEFD3-FDFD-491C-A266-F5A5A1EBD6B5}"_asset,

		// Constant deferred PBR material w. bending
		"8decad67-5f23-43aa-9e8b-78007fffe50f"_asset,

		// Masked textured deferred PBR material w. bending
		"f95a563e-10c2-43ef-b31e-38778e81af3d"_asset,

		// Two-sided masked textured deferred PBR material w. bending
		"4AC8A98E-EAA5-42A6-9367-C67F907D89EF"_asset,

		// Textured constant deferred PBR material w. bending
		"951348F8-8392-4A89-B49C-698591676775"_asset,

		// Two-sided Textured constant deferred PBR material w. bending
		"18F69209-F9E0-477A-927C-948B113DD41F"_asset,

		// Constant deferred PBR material
		"A897CDFA-A65F-4222-A5AB-D64DF68C17B6"_asset,

		// Two-sided Constant deferred PBR material
		"EC20C8F0-A98B-413B-B8B0-C98510888B13"_asset,

		// Constant deferred PBR + bloom material
		"40BCFD38-00D8-4D1C-B231-1F8F77B23C9A"_asset,

		// Textured constant scrolling deferred PBR material
		"2DF11F38-0B20-41C6-BE48-C5C5FB8C6F9E"_asset,

		// Two-sided Textured constant scrolling deferred PBR material
		"5A4C5C51-300F-468E-B167-3C00A23FB0FC"_asset,

		// Textured deferred scrolling PBR material
		"84D95EE2-20F7-49C9-BA82-9ADBE6A15864"_asset,

		// Two-sided textured deferred scrolling PBR material
		"3DB79A0B-1C7D-43BF-A571-80C47995B27E"_asset,

		// Textured deferred PBR + bloom + scrolling material
		"452a14ff-5060-40f3-a24d-805dd75ff5c9"_asset,

		// Deferred depth only material (used to set depth for the ground in AR to receive shadows)
		"8F6172ED-4A84-4137-8F33-97B49F05135E"_asset,
	};

	inline static constexpr Array DeferredSkyboxMaterialAssetGuids{
		// Texture skybox
		"D5E247DE-909A-43D2-A09F-B711E093D5CB"_asset,
		// Constant color skybox
		"D3B37153-2DCE-4574-A8F1-F118DF0E8E30"_asset,
	};

	enum class CommonMaterialStages : uint8
	{
		DrawTextToScene,
		Count
	};
	inline static constexpr Array<Asset::Guid, (uint8)CommonMaterialStages::Count> CommonMaterialStageAssetGuids{
		// Draw text to scene
		"1af4b974-d144-44fc-a391-e61fcb2156c0"_asset
	};

	inline static constexpr Array<Stage::Flags, (uint8)CommonMaterialStages::Count> CommonMaterialStageFlags{// Draw text to scene
	                                                                                                         Stage::Flags::Enabled
	};

	void SceneFramegraphBuilder::Create(SceneView& sceneView, Threading::JobBatch& jobBatchOut)
	{
		Threading::UniqueLock lock(m_mutex);

		const EnumFlags<PhysicalDeviceFeatures> supportedDeviceFeatures = sceneView.GetLogicalDevice().GetPhysicalDevice().GetSupportedFeatures(
		);
		if (supportedDeviceFeatures.IsSet(PhysicalDeviceFeatures::AccelerationStructure))
		{
			m_pBuildAccelerationStructureStage = EmplaceRenderStage<Rendering::BuildAccelerationStructureStage>(sceneView);
		}
		m_pMaterialIdentifiersStage = EmplaceRenderStage<Rendering::MaterialIdentifiersStage>(sceneView);
		m_pTilePopulationStage = EmplaceRenderStage<TilePopulationStage>(sceneView);

		const ShadowsStage::State shadowsState =
			supportedDeviceFeatures.AreAllSet(PhysicalDeviceFeatures::AccelerationStructure | PhysicalDeviceFeatures::RayQuery)
				? ShadowsStage::State::Raytraced
				: ShadowsStage::State::Rasterized;
		if (shadowsState != ShadowsStage::State::Disabled)
		{
			m_pShadowsStage = EmplaceRenderStage<ShadowsStage>(
				shadowsState,
				sceneView,
				m_pBuildAccelerationStructureStage,
				m_pTilePopulationStage->GetLightGatheringStage()
			);
		}

		if (ENABLE_SAMPLE_DISTRIBUTION_SHADOW_MAPS && shadowsState == ShadowsStage::State::Rasterized)
		{
			m_pDepthMinMaxPyramidStage = EmplaceRenderStage<DepthMinMaxPyramidStage>(sceneView, m_pShadowsStage);
		}
		m_pPBRLightingStage =
			EmplaceRenderStage<PBRLightingStage>(sceneView, m_pShadowsStage, *m_pTilePopulationStage, m_pBuildAccelerationStructureStage);
		if (Threading::JobBatch batch = m_pPBRLightingStage->LoadFixedResources())
		{
			jobBatchOut.QueueAfterStartStage(batch);
		}

		m_pSSAOStage = EmplaceRenderStage<SSAOStage>(sceneView);
		if (Threading::JobBatch batch = m_pSSAOStage->LoadFixedResources()) // Noise texture loaded here for now
		{
			jobBatchOut.QueueAfterStartStage(batch);
		}

		m_pSSRStage = EmplaceRenderStage<SSRStage>(sceneView);

		m_pPostProcessStage = EmplaceRenderStage<PostProcessStage>(sceneView);
		if (Threading::JobBatch batch = m_pPostProcessStage->LoadFixedResources())
		{
			jobBatchOut.QueueAfterStartStage(batch);
		}

		m_pDrawTextStage = EmplaceRenderStage<Font::DrawTextStage>(sceneView);

		{
			const uint16 nextMaterialIndex = m_materialStages.GetNextAvailableIndex();
			for (const Asset::Guid materialAssetGuid : DeferredMaterialAssetGuids)
			{
				EmplaceMaterialStage(sceneView, materialAssetGuid, Rendering::UpscalingFactor);
			}
			const ArrayView<UniquePtr<Rendering::MaterialStage>, uint16> newStages =
				m_materialStages.GetView().GetSubViewFrom(m_materialStages.begin() + nextMaterialIndex);
			m_deferredGBufferPopulationStages = {reinterpret_cast<ReferenceWrapper<Rendering::Stage>*>(newStages.GetData()), newStages.GetSize()};
		}
		{
			const uint16 nextMaterialIndex = m_materialStages.GetNextAvailableIndex();
			for (const Asset::Guid skyboxMaterialAssetGuid : DeferredSkyboxMaterialAssetGuids)
			{
				EmplaceMaterialStage(sceneView, skyboxMaterialAssetGuid, Rendering::UpscalingFactor);
			}
			const ArrayView<UniquePtr<Rendering::MaterialStage>, uint16> newStages =
				m_materialStages.GetView().GetSubViewFrom(m_materialStages.begin() + nextMaterialIndex);
			m_deferredSkyboxStages = {reinterpret_cast<ReferenceWrapper<Rendering::Stage>*>(newStages.GetData()), newStages.GetSize()};
		}

		{
			const uint16 nextMaterialIndex = m_materialStages.GetNextAvailableIndex();
			for (const Asset::Guid& commonMaterialAssetGuid : CommonMaterialStageAssetGuids)
			{
				const uint8 i = CommonMaterialStageAssetGuids.GetIteratorIndex(&commonMaterialAssetGuid);
				EmplaceMaterialStage(sceneView, commonMaterialAssetGuid, Rendering::UpscalingFactor, CommonMaterialStageFlags[i]);
			}
			const ArrayView<UniquePtr<Rendering::MaterialStage>, uint16> newStages =
				m_materialStages.GetView().GetSubViewFrom(m_materialStages.begin() + nextMaterialIndex);
			m_commonMaterialStages = {reinterpret_cast<ReferenceWrapper<Rendering::Stage>*>(newStages.GetData()), newStages.GetSize()};
		}

		m_deferredLightingStages.EmplaceBack(*m_pPBRLightingStage);

		if (sceneView.GetLogicalDevice().GetPhysicalDevice().GetSupportedFeatures().AreAnyNotSet(
					PhysicalDeviceFeatures::ReadWriteBuffers | PhysicalDeviceFeatures::ReadWriteTextures
				))
		{
			m_pCopyToScreenStage.CreateInPlace(sceneView.GetLogicalDevice(), "{08FF4EB4-CA68-4332-BAFF-01D4AB46FB35}"_guid);
		}
		else if constexpr (ENABLE_TAA && !ENABLE_FSR)
		{
			m_pCopyToScreenStage.CreateInPlace(sceneView.GetLogicalDevice(), "{08FF4EB4-CA68-4332-BAFF-01D4AB46FB35}"_guid);
			m_pCopyTemporalAAHistoryStage.CreateInPlace(sceneView.GetLogicalDevice(), "{11c582ee-1e28-4409-9347-00ef072f5117}"_guid);
		}
	}

	void SceneFramegraphBuilder::Build(Rendering::FramegraphBuilder& framegraphBuilder, SceneView& sceneView)
	{
		Threading::UniqueLock lock(m_mutex);

		const EnumFlags<Rendering::PhysicalDeviceFeatures> supportedDeviceFeatures =
			sceneView.GetLogicalDevice().GetPhysicalDevice().GetSupportedFeatures();
		const ShadowsStage::State shadowsState =
			supportedDeviceFeatures.AreAllSet(PhysicalDeviceFeatures::AccelerationStructure | PhysicalDeviceFeatures::RayQuery)
				? ShadowsStage::State::Raytraced
				: ShadowsStage::State::Rasterized;

		TextureCache& textureCache = System::Get<Rendering::Renderer>().GetTextureCache();

		const Math::Rectangleui outputArea = (Math::Rectangleui)sceneView.GetOutput().GetOutputArea();
		const Math::Vector2ui renderResolution = (Math::Vector2ui)(Math::Vector2f(outputArea.GetSize()) * UpscalingFactor);

		const Rendering::RenderTargetTemplateIdentifier renderOutputRenderTargetTemplateIdentifier =
			textureCache.FindOrRegisterRenderTargetTemplate(Framegraph::RenderOutputRenderTargetGuid);
		const Rendering::RenderTargetTemplateIdentifier mainDepthRenderTargetTemplateIdentifier =
			textureCache.FindOrRegisterRenderTargetTemplate(SceneViewDrawer::DefaultDepthStencilRenderTargetGuid);

		const Framegraph::StageIndex octreeTraversalStageIndex = framegraphBuilder.GetNextAvailableStageIndex();
		framegraphBuilder.EmplaceStage(
			Framegraph::GenericStageDescription{"Octree traversal", Framegraph::InvalidStageIndex, sceneView.GetOctreeTraversalStage()}
		);
		const Framegraph::StageIndex lateStageVisibilityCheckStageIndex = framegraphBuilder.GetNextAvailableStageIndex();
		framegraphBuilder.EmplaceStage(Framegraph::GenericStageDescription{
			"Late stage visibility check",
			octreeTraversalStageIndex,
			sceneView.GetLateStageVisibilityCheckStage()
		});

		Framegraph::StageIndex buildAccelerationStructureStageIndex = Framegraph::InvalidStageIndex;
		if (m_pBuildAccelerationStructureStage.IsValid())
		{
			buildAccelerationStructureStageIndex = framegraphBuilder.GetNextAvailableStageIndex();
			framegraphBuilder.EmplaceStage(Framegraph::GenericStageDescription{
				"Build acceleration structure",
				lateStageVisibilityCheckStageIndex,
				*m_pBuildAccelerationStructureStage
			});
		}

		SceneViewDrawer& sceneViewDrawer = sceneView.GetDrawer();
		sceneView.GetMaterialsStage().SetForwardingStage(*m_pMaterialIdentifiersStage);
		framegraphBuilder.EmplaceStage(Framegraph::RenderPassStageDescription{
			"Material Identifiers",
			lateStageVisibilityCheckStageIndex,
			m_pMaterialIdentifiersStage,
			sceneViewDrawer,
			Math::Rectangleui{Math::Zero, renderResolution},
			MATERIAL_IDENTIFIERS_DEPTH_ONLY ? ArrayView<const Framegraph::ColorAttachmentDescription, Framegraph::AttachmentIndex>{}
																			: framegraphBuilder.EmplaceColorAttachments(Array{
																					Framegraph::ColorAttachmentDescription{
																						renderOutputRenderTargetTemplateIdentifier,
																						renderResolution,
																						MipRange(0, 1),
																						ArrayRange(0, 1),
																						Framegraph::AttachmentFlags::CanStore,
																						Optional<Math::Color>{}
																					},
																				}),
			Framegraph::DepthAttachmentDescription{
				mainDepthRenderTargetTemplateIdentifier,
				renderResolution,
				MipRange(0, 1),
				ArrayRange(0, 1),
				Framegraph::AttachmentFlags::Clear | Framegraph::AttachmentFlags::CanStore,
				DepthValue{0.f}
			},
			Optional<Framegraph::StencilAttachmentDescription>{}
		});

		// TODO: Would be cool to get an event when all lights have been found.
		// We already have masks in octree nodes so we could get an early notice for all processing complete before other objects are culled.
		uint16 hierarchicalDepthMipCount{0};
		if (shadowsState != ShadowsStage::State::Disabled)
		{
			const bool supportsGeometryShaderLayeredShadows =
				supportedDeviceFeatures.AreAllSet(PhysicalDeviceFeatures::GeometryShader | PhysicalDeviceFeatures::LayeredRendering);

			if (ENABLE_SAMPLE_DISTRIBUTION_SHADOW_MAPS && shadowsState == ShadowsStage::State::Rasterized)
			{
				Math::Vector2ui screenResolution = renderResolution;

				screenResolution = Math::Max(screenResolution >> Math::Vector2ui{1}, Math::Vector2ui{1u});
				Rendering::MipMask mipMask = Rendering::MipMask::FromSizeAllToLargest(screenResolution);
				hierarchicalDepthMipCount = mipMask.GetSize();

				FlatVector<Framegraph::ComputeSubpassDescription, 32> subpasses;
				subpasses.EmplaceBack(Framegraph::ComputeSubpassDescription{
					"Main depth to first mip",
					framegraphBuilder.EmplaceSubpassAttachmentReferences(Array{Framegraph::SubpassOutputAttachmentReference{0, MipRange(0, 1)}}),
					ArrayView<const Framegraph::SubpassInputOutputAttachmentReference, Framegraph::AttachmentIndex>{},
					framegraphBuilder.EmplaceSubpassAttachmentReferences(Array{Framegraph::SubpassInputAttachmentReference{1, MipRange(0, 1)}})
				});

				for (MipMask::StoredType mipLevel = 1; mipLevel < hierarchicalDepthMipCount; ++mipLevel)
				{
					subpasses.EmplaceBack(Framegraph::ComputeSubpassDescription{
						"Pyramid reduction",
						framegraphBuilder
							.EmplaceSubpassAttachmentReferences(Array{Framegraph::SubpassOutputAttachmentReference{0, MipRange(mipLevel, 1)}}),
						ArrayView<const Framegraph::SubpassInputOutputAttachmentReference, Framegraph::AttachmentIndex>{},
						framegraphBuilder
							.EmplaceSubpassAttachmentReferences(Array{Framegraph::SubpassInputAttachmentReference{0, MipRange(mipLevel - 1, 1)}})
					});
				}

				const auto depthMinMaxSubpasses = framegraphBuilder.EmplaceComputeSubpass(subpasses.GetView());

				framegraphBuilder.EmplaceStage(Framegraph::ComputePassStageDescription{
					"Depth min max pyramid reduction",
					lateStageVisibilityCheckStageIndex,
					*m_pDepthMinMaxPyramidStage,
					sceneViewDrawer,
					framegraphBuilder.EmplaceOutputAttachments(Array{Framegraph::OutputAttachmentDescription{
						textureCache.FindOrRegisterRenderTargetTemplate(DepthMinMaxPyramidStage::RenderTargetAssetGuid),
						screenResolution,
						ImageSubresourceRange{ImageAspectFlags::Color, MipRange(0, hierarchicalDepthMipCount), ArrayRange(0, 1)}
					}}),
					ArrayView<const Framegraph::InputOutputAttachmentDescription, Framegraph::AttachmentIndex>{},
					framegraphBuilder.EmplaceInputAttachments(Array{Framegraph::InputAttachmentDescription{
						mainDepthRenderTargetTemplateIdentifier,
						renderResolution,
						ImageSubresourceRange{ImageAspectFlags::Depth, MipRange(0, 1), ArrayRange(0, 1)}
					}}),
					depthMinMaxSubpasses
				});

				FlatVector<Framegraph::SubpassOutputAttachmentReference, ShadowsStage::MaximumShadowmapCount> shadowOutputAttachmentReferences;

				if (supportsGeometryShaderLayeredShadows)
				{
					shadowOutputAttachmentReferences.EmplaceBack(Framegraph::SubpassOutputAttachmentReference{
						0,
						MipRange(0, 1),
						ArrayRange(0, ShadowsStage::MaximumShadowmapCount),
						ImageMappingType::TwoDimensionalArray
					});
				}
				else
				{
					for (uint8 i = 0; i < ShadowsStage::MaximumShadowmapCount; ++i)
					{
						shadowOutputAttachmentReferences.EmplaceBack(
							Framegraph::SubpassOutputAttachmentReference{0, MipRange(0, 1), ArrayRange(i, 1), ImageMappingType::TwoDimensional}
						);
					}
				}

				framegraphBuilder.EmplaceStage(Framegraph::GenericStageDescription{
					"Shadows",
					lateStageVisibilityCheckStageIndex,
					*m_pShadowsStage,
					framegraphBuilder.EmplaceOutputAttachments(Array{Framegraph::OutputAttachmentDescription{
						textureCache.FindOrRegisterRenderTargetTemplate(ShadowsStage::RenderTargetGuid),
						Math::Vector2ui{ShadowsStage::ShadowMapSize},
						ImageSubresourceRange{ImageAspectFlags::Depth, MipRange(0, 1), ArrayRange(0, ShadowsStage::MaximumShadowmapCount)}
					}}),
					ArrayView<const Framegraph::InputOutputAttachmentDescription, Framegraph::AttachmentIndex>{},
					framegraphBuilder.EmplaceInputAttachments(Array{
						Framegraph::InputAttachmentDescription{
							textureCache.FindOrRegisterRenderTargetTemplate(DepthMinMaxPyramidStage::RenderTargetAssetGuid),
							outputArea.GetSize(),
							ImageSubresourceRange{ImageAspectFlags::Color, MipRange(1, 1), ArrayRange(0, 1)}
						},
						Framegraph::InputAttachmentDescription{
							textureCache.FindOrRegisterRenderTargetTemplate(DepthMinMaxPyramidStage::RenderTargetAssetGuid),
							outputArea.GetSize(),
							ImageSubresourceRange{ImageAspectFlags::Color, MipRange(hierarchicalDepthMipCount - 1, 1), ArrayRange(0, 1)}
						}
					}),
					Framegraph::GenericSubpassDescription{
						"Shadows",
						framegraphBuilder.EmplaceSubpassAttachmentReferences(shadowOutputAttachmentReferences),
						ArrayView<const Framegraph::SubpassInputOutputAttachmentReference, Framegraph::AttachmentIndex>{},
						framegraphBuilder.EmplaceSubpassAttachmentReferences(Array{
							Framegraph::SubpassInputAttachmentReference{1, MipRange(1, 1), ArrayRange(0, 1), ImageMappingType::TwoDimensional},
							Framegraph::SubpassInputAttachmentReference{
								2,
								MipRange(hierarchicalDepthMipCount - 1, 1),
								ArrayRange(0, 1),
								ImageMappingType::TwoDimensional
							}
						})
					}
				});
			}
		}

		const Math::Vector2ui tilesResolution =
			(Math::Vector2ui)(Math::Vector2f(TilePopulationStage::CalculateTileSize(outputArea.GetSize())) * UpscalingFactor);

		framegraphBuilder.EmplaceStage(Framegraph::ComputePassStageDescription{
			"Deferred light tile population",
			lateStageVisibilityCheckStageIndex,
			m_pTilePopulationStage,
			sceneViewDrawer,
			framegraphBuilder.EmplaceOutputAttachments(Array{Framegraph::OutputAttachmentDescription{
				textureCache.FindOrRegisterRenderTargetTemplate(TilePopulationStage::ClustersTextureAssetGuid),
				tilesResolution,
				ImageSubresourceRange{ImageAspectFlags::Color, MipRange(0, 1), ArrayRange(0, 1)}
			}}),
			ArrayView<const Framegraph::InputOutputAttachmentDescription, Framegraph::AttachmentIndex>{},
			ArrayView<const Framegraph::InputAttachmentDescription, Framegraph::AttachmentIndex>{},
			framegraphBuilder.EmplaceComputeSubpass(Array{Framegraph::ComputeSubpassDescription{
				"Deferred light tile population",
				framegraphBuilder.EmplaceSubpassAttachmentReferences(
					Array{Framegraph::SubpassOutputAttachmentReference{(Framegraph::AttachmentIndex)0, MipRange(0, 1)}}
				),
				ArrayView<const Framegraph::SubpassInputOutputAttachmentReference, Framegraph::AttachmentIndex>{},
				ArrayView<const Framegraph::SubpassInputAttachmentReference, Framegraph::AttachmentIndex>{},
			}})
		});

		// Deferred pass
		enum class DeferredPassAttachment : uint8
		{
			Albedo,
			Normals,
			MaterialProperties,
			VelocityVectors,
			HDR,
			Depth,
			Clusters,
			Shadowmaps // Only when rasterized and not raytraced
		};

		static const Array gbufferPopulationColorAttachments{
			(Framegraph::AttachmentIndex)DeferredPassAttachment::Albedo,
			(Framegraph::AttachmentIndex)DeferredPassAttachment::Normals,
			(Framegraph::AttachmentIndex)DeferredPassAttachment::MaterialProperties,
			(Framegraph::AttachmentIndex)DeferredPassAttachment::VelocityVectors,
		};

		static const Array deferredLightingColorAttachments{
			(Framegraph::AttachmentIndex)DeferredPassAttachment::HDR,
		};
		static const Array deferredLightingSubpassInputAttachments{
			(Framegraph::AttachmentIndex)DeferredPassAttachment::Albedo,
			(Framegraph::AttachmentIndex)DeferredPassAttachment::Normals,
			(Framegraph::AttachmentIndex)DeferredPassAttachment::MaterialProperties,
			(Framegraph::AttachmentIndex)DeferredPassAttachment::Depth,
		};

		FlatVector<Framegraph::InputAttachmentDescription, 2> deferredLightingExternalInputAttachmentDescriptions{
			Framegraph::InputAttachmentDescription{
				textureCache.FindOrRegisterRenderTargetTemplate(TilePopulationStage::ClustersTextureAssetGuid),
				tilesResolution,
				ImageSubresourceRange{
					ImageAspectFlags::Color,
					MipRange(0, 1),
					ArrayRange(0, 1),
				}
			}
		};
		FlatVector<Rendering::Framegraph::AttachmentIndex, 2> deferredLightingExternalInputAttachments{(Framegraph::AttachmentIndex
		)DeferredPassAttachment::Clusters};
		if (shadowsState == ShadowsStage::State::Rasterized)
		{
			deferredLightingExternalInputAttachmentDescriptions.EmplaceBack(Framegraph::InputAttachmentDescription{
				textureCache.FindOrRegisterRenderTargetTemplate(ShadowsStage::RenderTargetGuid),
				Math::Vector2ui{ShadowsStage::ShadowMapSize},
				ImageSubresourceRange{ImageAspectFlags::Depth, MipRange(0, 1), ArrayRange(0, ShadowsStage::MaximumShadowmapCount)},
				ImageMappingType::TwoDimensionalArray
			});
			deferredLightingExternalInputAttachments.EmplaceBack((Framegraph::AttachmentIndex)DeferredPassAttachment::Shadowmaps);
		}

		static const Array deferredSkyboxColorAttachments{
			(Framegraph::AttachmentIndex)DeferredPassAttachment::HDR,
		};

		framegraphBuilder.EmplaceStage(Framegraph::ExplicitRenderPassStageDescription{
			"Deferred Lighting",
			buildAccelerationStructureStageIndex != Framegraph::InvalidStageIndex ? buildAccelerationStructureStageIndex
																																						: lateStageVisibilityCheckStageIndex,
			Invalid,
			sceneViewDrawer,
			Math::Rectangleui{Math::Zero, renderResolution},
			framegraphBuilder.EmplaceColorAttachments(Array{
				// Albedo
				Framegraph::ColorAttachmentDescription{
					textureCache.FindOrRegisterRenderTargetTemplate("90d3d032-2e1e-4d72-b3c0-d3b4114c9d2e"_asset),
					renderResolution,
					MipRange(0, 1),
					ArrayRange(0, 1),
					Framegraph::AttachmentFlags::CanStore,
					Optional<Math::Color>{}
				},
				// Normals
				Framegraph::ColorAttachmentDescription{
					textureCache.FindOrRegisterRenderTargetTemplate("18988bce-dd7a-4d4a-9606-741047372c44"_asset),
					renderResolution,
					MipRange(0, 1),
					ArrayRange(0, 1),
					Framegraph::AttachmentFlags::CanStore,
					Optional<Math::Color>{}
				},
				// Material properties
				Framegraph::ColorAttachmentDescription{
					textureCache.FindOrRegisterRenderTargetTemplate("fda607dc-ed70-4417-a242-84c3f6a86377"_asset),
					renderResolution,
					MipRange(0, 1),
					ArrayRange(0, 1),
					Framegraph::AttachmentFlags::CanStore,
					Optional<Math::Color>{}
				},
				// Velocity vectors
				Framegraph::ColorAttachmentDescription{
					textureCache.FindOrRegisterRenderTargetTemplate(PostProcessStage::TAAVelocityRenderTargetAssetGuid),
					renderResolution,
					MipRange(0, 1),
					ArrayRange(0, 1),
					Framegraph::AttachmentFlags::CanStore,
					Math::Color{0.f, 0.f, 0.f, 0.f}
				},
				// Render output (HDR)
				Framegraph::ColorAttachmentDescription{
					textureCache.FindOrRegisterRenderTargetTemplate(PBRLightingStage::HDRSceneRenderTargetAssetGuid),
					renderResolution,
					MipRange(0, 1),
					ArrayRange(0, 1),
					Framegraph::AttachmentFlags::CanStore,
					Math::Color{0.f, 0.f, 0.f, 0.f},
				}
			}),
			Framegraph::DepthAttachmentDescription{
				mainDepthRenderTargetTemplateIdentifier,
				renderResolution,
				MipRange(0, 1),
				ArrayRange(0, 1),
				Framegraph::AttachmentFlags::Clear | Framegraph::AttachmentFlags::CanStore,
				DepthValue{0.f}
			},
			Optional<Framegraph::StencilAttachmentDescription>{},
			framegraphBuilder.EmplaceInputAttachments(deferredLightingExternalInputAttachmentDescriptions),
			framegraphBuilder.EmplaceRenderSubpass(Array{
				Framegraph::RenderSubpassDescription{
					"Textured deferred PBR material",
					m_deferredGBufferPopulationStages,
					gbufferPopulationColorAttachments,
					(Framegraph::AttachmentIndex)DeferredPassAttachment::Depth,
					Framegraph::InvalidAttachmentIndex,
				},
				Framegraph::RenderSubpassDescription{
					"Deferred lighting",
					m_deferredLightingStages.GetView(),
					deferredLightingColorAttachments,
					Framegraph::InvalidAttachmentIndex,
					Framegraph::InvalidAttachmentIndex,
					deferredLightingSubpassInputAttachments,
					framegraphBuilder.EmplaceAttachmentIndices(deferredLightingExternalInputAttachments.GetView())
				},
				Framegraph::RenderSubpassDescription{
					"Deferred skybox",
					m_deferredSkyboxStages,
					deferredSkyboxColorAttachments,
					(Framegraph::AttachmentIndex)DeferredPassAttachment::Depth,
					Framegraph::InvalidAttachmentIndex
				}
			})
		});

		if constexpr (true) //(!PLATFORM_WEB && !PLATFORM_APPLE_VISIONOS)
		{
			framegraphBuilder.EmplaceStage(Framegraph::ComputePassStageDescription{
				"Post-lighting SSAO",
				lateStageVisibilityCheckStageIndex,
				m_pSSAOStage,
				sceneViewDrawer,
				ArrayView<const Framegraph::OutputAttachmentDescription, Framegraph::AttachmentIndex>{},
				// Depth/Normal/Noise -> SSAO.comp -> SSAO -> Blur.comp (H) -> SSAO Blur -> Blur.comp (V) -> SSAO
			  // framegraphBuilder.EmplaceOutputAttachments(Array{
			  // 	Framegraph::OutputAttachmentDescription{
			  // 		textureCache.FindOrRegisterRenderTargetTemplate(SSAOStage::RenderTargetAssetGuid), // SSAO Buffer
			  // 		outputArea.GetSize(),
			  // 		ImageSubresourceRange{ImageAspectFlags::Color, MipRange(0, 1), ArrayRange(0, 1)}
			  // 	},
			  // 	Framegraph::OutputAttachmentDescription{
			  // 		textureCache.FindOrRegisterRenderTargetTemplate(SSAOStage::RenderTargetBlurAssetGuid), // SSAO Blur Buffer
			  // 		outputArea.GetSize(),
			  // 		ImageSubresourceRange{ImageAspectFlags::Color, MipRange(0, 1), ArrayRange(0, 1)}
			  // 	}
			  // }),
			  // ArrayView<const Framegraph::InputOutputAttachmentDescription, Framegraph::AttachmentIndex>{},
				framegraphBuilder.EmplaceInputOutputAttachments(Array{Framegraph::InputOutputAttachmentDescription{
					textureCache.FindOrRegisterRenderTargetTemplate(PBRLightingStage::HDRSceneRenderTargetAssetGuid),
					renderResolution,
					ImageSubresourceRange{ImageAspectFlags::Color, MipRange(0, 1), ArrayRange(0, 1)}
				}}),
				// Depth, Normals, and Noise Texture (TODO, loaded old way in stage file for now)
				framegraphBuilder.EmplaceInputAttachments(Array{
					Framegraph::InputAttachmentDescription{
						mainDepthRenderTargetTemplateIdentifier,
						renderResolution,
						ImageSubresourceRange{ImageAspectFlags::Depth, MipRange(0, 1), ArrayRange(0, 1)}
					},
					Framegraph::InputAttachmentDescription{
						textureCache.FindOrRegisterRenderTargetTemplate("18988bce-dd7a-4d4a-9606-741047372c44"_asset),
						renderResolution,
						ImageSubresourceRange{ImageAspectFlags::Color, MipRange(0, 1), ArrayRange(0, 1)}
					}
				}),
				framegraphBuilder.EmplaceComputeSubpass(Array{
					Framegraph::ComputeSubpassDescription{
						"SSAO",
						ArrayView<const Framegraph::SubpassOutputAttachmentReference, Framegraph::AttachmentIndex>{},
						// framegraphBuilder.EmplaceSubpassAttachmentReferences(
			      // 	Array{Framegraph::SubpassOutputAttachmentReference{(Framegraph::AttachmentIndex)SSAOStage::Images::SSAO, MipRange(0, 1)}}
			      // ),
			      // ArrayView<const SubpassInputOutputAttachmentReference, Framegraph::AttachmentIndex>{},
						framegraphBuilder.EmplaceSubpassAttachmentReferences(
							Array{Framegraph::SubpassInputOutputAttachmentReference{(Framegraph::AttachmentIndex)SSAOStage::Images::HDR, MipRange(0, 1)}}
						),
						framegraphBuilder.EmplaceSubpassAttachmentReferences(Array{
							Framegraph::SubpassInputAttachmentReference{(Framegraph::AttachmentIndex)SSAOStage::Images::Depth, MipRange(0, 1)},
							Framegraph::SubpassInputAttachmentReference{(Framegraph::AttachmentIndex)SSAOStage::Images::Normals, MipRange(0, 1)},
							// TODO: Add LensDirt here when we do textures in framegraph
						}),
					},
					// Framegraph::ComputeSubpassDescription{
			    // 	"Simple Blur",
			    // 	framegraphBuilder.EmplaceSubpassAttachmentReferences(
			    // 		Array{Framegraph::SubpassOutputAttachmentReference{(Framegraph::AttachmentIndex)SSAOStage::Images::SSAOBlur, MipRange(0,
			    // 1)}}
			    // 	),
			    // 	ArrayView<const SubpassInputOutputAttachmentReference, Framegraph::AttachmentIndex>{},
			    // 	framegraphBuilder.EmplaceSubpassAttachmentReferences(Array{Framegraph::SubpassInputAttachmentReference{(Framegraph::AttachmentIndex)SSAOStage::Images::SSAO,
			    // MipRange(0, 1)}
			    // 	}),
			    // }
				})
			});
		}

		if constexpr (false)
		{
			framegraphBuilder.EmplaceStage(Framegraph::ComputePassStageDescription{
				"SSR",
				Framegraph::InvalidStageIndex,
				m_pSSRStage,
				sceneViewDrawer,
				framegraphBuilder.EmplaceOutputAttachments(Array{Framegraph::OutputAttachmentDescription{
					textureCache.FindOrRegisterRenderTargetTemplate(SSRStage::RenderTargetAssetGuid),
					renderResolution,
					ImageSubresourceRange{ImageAspectFlags::Color, MipRange(0, 1), ArrayRange(0, 1)}
				}}),
				framegraphBuilder.EmplaceInputOutputAttachments(Array{Framegraph::InputOutputAttachmentDescription{
					textureCache.FindOrRegisterRenderTargetTemplate(PBRLightingStage::HDRSceneRenderTargetAssetGuid),
					renderResolution,
					ImageSubresourceRange{ImageAspectFlags::Color, MipRange(0, 1), ArrayRange(0, 1)}
				}}),
				framegraphBuilder.EmplaceInputAttachments(Array{
					Framegraph::InputAttachmentDescription{
						mainDepthRenderTargetTemplateIdentifier,
						renderResolution,
						ImageSubresourceRange{ImageAspectFlags::Depth, MipRange(0, 1), ArrayRange(0, 1)}
					},
					// GBuffer normals
					Framegraph::InputAttachmentDescription{
						textureCache.FindOrRegisterRenderTargetTemplate("18988bce-dd7a-4d4a-9606-741047372c44"_asset),
						renderResolution,
						ImageSubresourceRange{ImageAspectFlags::Color, MipRange(0, 1), ArrayRange(0, 1)}
					},
					// GBuffer material properties
					Framegraph::InputAttachmentDescription{
						textureCache.FindOrRegisterRenderTargetTemplate("fda607dc-ed70-4417-a242-84c3f6a86377"_asset),
						renderResolution,
						ImageSubresourceRange{ImageAspectFlags::Color, MipRange(0, 1), ArrayRange(0, 1)}
					}
				}),
				framegraphBuilder.EmplaceComputeSubpass(Array{
					Framegraph::ComputeSubpassDescription{
						"SSR",
						framegraphBuilder.EmplaceSubpassAttachmentReferences(Array{Framegraph::SubpassOutputAttachmentReference{0, MipRange(0, 1)}}),
						ArrayView<const Framegraph::SubpassInputOutputAttachmentReference, Framegraph::AttachmentIndex>{},
						framegraphBuilder.EmplaceSubpassAttachmentReferences(Array{
							Framegraph::SubpassOutputAttachmentReference{1, MipRange(0, 1)},
							Framegraph::SubpassOutputAttachmentReference{2, MipRange(0, 1)},
							Framegraph::SubpassOutputAttachmentReference{3, MipRange(0, 1)},
							Framegraph::SubpassOutputAttachmentReference{4, MipRange(0, 1)}
						}),
					},
					Framegraph::ComputeSubpassDescription{
						"SSR Composite",
						framegraphBuilder.EmplaceSubpassAttachmentReferences(Array{Framegraph::SubpassOutputAttachmentReference{1, MipRange(0, 1)}}),
						ArrayView<const Framegraph::SubpassInputOutputAttachmentReference, Framegraph::AttachmentIndex>{},
						framegraphBuilder.EmplaceSubpassAttachmentReferences(Array{Framegraph::SubpassOutputAttachmentReference{0, MipRange(0, 1)}}),
					},
				})
			});
		}

		// Draw text to world
		const Framegraph::StageIndex drawTextToRenderTargetStageIndex = framegraphBuilder.GetNextAvailableStageIndex();
		framegraphBuilder.EmplaceStage(
			Framegraph::GenericStageDescription{"Draw text to rendertarget", lateStageVisibilityCheckStageIndex, *m_pDrawTextStage}
		);

		framegraphBuilder.EmplaceStage(Framegraph::RenderPassStageDescription{
			"Draw text to scene",
			drawTextToRenderTargetStageIndex,
			*m_commonMaterialStages[(uint8)CommonMaterialStages::DrawTextToScene],
			sceneViewDrawer,
			Math::Rectangleui{Math::Zero, renderResolution},
			framegraphBuilder.EmplaceColorAttachments(Array{
				Framegraph::ColorAttachmentDescription{
					textureCache.FindOrRegisterRenderTargetTemplate(PBRLightingStage::HDRSceneRenderTargetAssetGuid),
					renderResolution,
					MipRange(0, 1),
					ArrayRange(0, 1),
					Framegraph::AttachmentFlags::CanRead | Framegraph::AttachmentFlags::CanStore,
					Optional<Math::Color>{}
				},
			}),
			Framegraph::DepthAttachmentDescription{
				mainDepthRenderTargetTemplateIdentifier,
				renderResolution,
				MipRange(0, 1),
				ArrayRange(0, 1),
				Framegraph::AttachmentFlags::CanRead | Framegraph::AttachmentFlags::CanStore,
				Optional<DepthValue>{}
			}
		});

		// copy HDR scene to screen
		if (sceneView.GetLogicalDevice().GetPhysicalDevice().GetSupportedFeatures().AreAnyNotSet(
					PhysicalDeviceFeatures::ReadWriteBuffers | PhysicalDeviceFeatures::ReadWriteTextures
				))
		{
			framegraphBuilder.EmplaceStage(Framegraph::RenderPassStageDescription{
				"copy HDR to screen",
				Framegraph::InvalidStageIndex,
				*m_pCopyToScreenStage,
				sceneViewDrawer,
				outputArea,
				framegraphBuilder.EmplaceColorAttachments(Array{
					Framegraph::ColorAttachmentDescription{
						renderOutputRenderTargetTemplateIdentifier,
						outputArea.GetSize(),
						MipRange(0, 1),
						ArrayRange(0, 1),
						Framegraph::AttachmentFlags::CanRead | Framegraph::AttachmentFlags::CanStore,
						Optional<Math::Color>{}
					},
				}),
				Optional<Framegraph::DepthAttachmentDescription>{},
				Optional<Framegraph::StencilAttachmentDescription>{},
				framegraphBuilder.EmplaceInputAttachments(Array{Framegraph::InputAttachmentDescription{
					textureCache.FindOrRegisterRenderTargetTemplate(PBRLightingStage::HDRSceneRenderTargetAssetGuid),
					outputArea.GetSize(),
					ImageSubresourceRange{ImageAspectFlags::Color, MipRange(0, 1), ArrayRange(0, 1)}
				}})
			});
		}
		else
		{
			framegraphBuilder.EmplaceStage(Framegraph::ComputePassStageDescription{
				"Post Process",
				lateStageVisibilityCheckStageIndex,
				m_pPostProcessStage,
				sceneViewDrawer,
				framegraphBuilder.EmplaceOutputAttachments(Array{
					Framegraph::OutputAttachmentDescription{
						renderOutputRenderTargetTemplateIdentifier,
						outputArea.GetSize(),
						ImageSubresourceRange{ImageAspectFlags::Color, MipRange(0, 1), ArrayRange(0, 1)}
					},
					Framegraph::OutputAttachmentDescription{
						textureCache.FindOrRegisterRenderTargetTemplate(PostProcessStage::DownsampleRenderTargetAssetGuid),
						(Math::Vector2ui)(Math::Vector2f(outputArea.GetSize() / PostProcessStage::LensFlareResolutionDivider) * UpscalingFactor),
						ImageSubresourceRange{ImageAspectFlags::Color, MipRange(0, 1), ArrayRange(0, 1)}
					},
					Framegraph::OutputAttachmentDescription{
						textureCache.FindOrRegisterRenderTargetTemplate(PostProcessStage::LensFlareRenderTargetAssetGuid),
						(Math::Vector2ui)(Math::Vector2f(outputArea.GetSize() / PostProcessStage::LensFlareResolutionDivider) * UpscalingFactor),
						ImageSubresourceRange{ImageAspectFlags::Color, MipRange(0, 1), ArrayRange(0, 1)}
					},
					Framegraph::OutputAttachmentDescription{
						textureCache.FindOrRegisterRenderTargetTemplate(PostProcessStage::CompositeSceneRenderTargetAssetGuid),
						renderResolution,
						ImageSubresourceRange{ImageAspectFlags::Color, MipRange(0, 1), ArrayRange(0, 1)}
					},
					Framegraph::OutputAttachmentDescription{
						textureCache.FindOrRegisterRenderTargetTemplate(PostProcessStage::TAAHistorySceneRenderTargetAssetGuid),
						renderResolution,
						ImageSubresourceRange{ImageAspectFlags::Color, MipRange(0, 1), ArrayRange(0, 1)}
					},
					Framegraph::OutputAttachmentDescription{
						textureCache.FindOrRegisterRenderTargetTemplate(PostProcessStage::SuperResolutionSceneRenderTargetAssetGuid),
						outputArea.GetSize(),
						ImageSubresourceRange{ImageAspectFlags::Color, MipRange(0, 1), ArrayRange(0, 1)}
					}
				}),
				ArrayView<const Framegraph::InputOutputAttachmentDescription, Framegraph::AttachmentIndex>{},
				framegraphBuilder.EmplaceInputAttachments(Array{
					Framegraph::InputAttachmentDescription{
						textureCache.FindOrRegisterRenderTargetTemplate(PBRLightingStage::HDRSceneRenderTargetAssetGuid),
						renderResolution,
						ImageSubresourceRange{ImageAspectFlags::Color, MipRange(0, 1), ArrayRange(0, 1)}
					},
					Framegraph::InputAttachmentDescription{
						textureCache.FindOrRegisterRenderTargetTemplate(PostProcessStage::TAAVelocityRenderTargetAssetGuid),
						renderResolution,
						ImageSubresourceRange{ImageAspectFlags::Color, MipRange(0, 1), ArrayRange(0, 1)}
					}
				}),
				framegraphBuilder.EmplaceComputeSubpass(Array {
					Framegraph::ComputeSubpassDescription{
						"Downsample",
						framegraphBuilder.EmplaceSubpassAttachmentReferences(Array{Framegraph::SubpassOutputAttachmentReference{
							(Framegraph::AttachmentIndex)PostProcessStage::Images::Downsampled,
							MipRange(0, 1)
						}}),
						ArrayView<const Framegraph::SubpassInputOutputAttachmentReference, Framegraph::AttachmentIndex>{},
						framegraphBuilder.EmplaceSubpassAttachmentReferences(
							Array{Framegraph::SubpassInputAttachmentReference{(Framegraph::AttachmentIndex)PostProcessStage::Images::HDR, MipRange(0, 1)}}
						),
					},
						Framegraph::ComputeSubpassDescription{
							"Lensflare Generation",
							framegraphBuilder.EmplaceSubpassAttachmentReferences(Array{Framegraph::SubpassOutputAttachmentReference{
								(Framegraph::AttachmentIndex)PostProcessStage::Images::LensFlare,
								MipRange(0, 1)
							}}),
							ArrayView<const Framegraph::SubpassInputOutputAttachmentReference, Framegraph::AttachmentIndex>{},
							framegraphBuilder.EmplaceSubpassAttachmentReferences(Array{Framegraph::SubpassInputAttachmentReference{
								(Framegraph::AttachmentIndex)PostProcessStage::Images::Downsampled,
								MipRange(0, 1)
							}}),
						},
						Framegraph::ComputeSubpassDescription{
							"Lensflare Horizontal Blur",
							framegraphBuilder.EmplaceSubpassAttachmentReferences(Array{Framegraph::SubpassOutputAttachmentReference{
								(Framegraph::AttachmentIndex)PostProcessStage::Images::Downsampled,
								MipRange(0, 1)
							}}),
							ArrayView<const Framegraph::SubpassInputOutputAttachmentReference, Framegraph::AttachmentIndex>{},
							framegraphBuilder.EmplaceSubpassAttachmentReferences(Array{Framegraph::SubpassInputAttachmentReference{
								(Framegraph::AttachmentIndex)PostProcessStage::Images::LensFlare,
								MipRange(0, 1)
							}}),
						},
						Framegraph::ComputeSubpassDescription{
							"Lensflare Vertical Blur",
							framegraphBuilder.EmplaceSubpassAttachmentReferences(Array{Framegraph::SubpassOutputAttachmentReference{
								(Framegraph::AttachmentIndex)PostProcessStage::Images::LensFlare,
								MipRange(0, 1)
							}}),
							ArrayView<const Framegraph::SubpassInputOutputAttachmentReference, Framegraph::AttachmentIndex>{},
							framegraphBuilder.EmplaceSubpassAttachmentReferences(Array{Framegraph::SubpassInputAttachmentReference{
								(Framegraph::AttachmentIndex)PostProcessStage::Images::Downsampled,
								MipRange(0, 1)
							}}),
						},
						Framegraph::ComputeSubpassDescription{
							"Composite",
							framegraphBuilder.EmplaceSubpassAttachmentReferences(Array{Framegraph::SubpassOutputAttachmentReference{
								sceneView.GetLogicalDevice().GetPhysicalDevice().GetSupportedFeatures().AreAllSet(
									PhysicalDeviceFeatures::ReadWriteBuffers | PhysicalDeviceFeatures::ReadWriteTextures
								)
									? (Framegraph::AttachmentIndex)PostProcessStage::Images::Composite
									: (Framegraph::AttachmentIndex)PostProcessStage::Images::RenderOutput,
								MipRange(0, 1)
							}}),
							ArrayView<const Framegraph::SubpassInputOutputAttachmentReference, Framegraph::AttachmentIndex>{},
							framegraphBuilder.EmplaceSubpassAttachmentReferences(Array{
								Framegraph::SubpassInputAttachmentReference{(Framegraph::AttachmentIndex)PostProcessStage::Images::HDR, MipRange(0, 1)},
								Framegraph::SubpassInputAttachmentReference{
									(Framegraph::AttachmentIndex)PostProcessStage::Images::LensFlare,
									MipRange(0, 1)
								}
								// TODO: Add LensDirt here when we do textures in framegraph
							}),
						},
#if ENABLE_TAA
						Framegraph::ComputeSubpassDescription{
							"Temporal AA Resolve",
							framegraphBuilder.EmplaceSubpassAttachmentReferences(Array{
								Framegraph::SubpassOutputAttachmentReference{(Framegraph::AttachmentIndex)PostProcessStage::Images::HDR, MipRange(0, 1)}
							}),
							ArrayView<const Framegraph::SubpassInputOutputAttachmentReference, Framegraph::AttachmentIndex>{},
							framegraphBuilder.EmplaceSubpassAttachmentReferences(Array{
								Framegraph::SubpassInputAttachmentReference{
									(Framegraph::AttachmentIndex)PostProcessStage::Images::Composite,
									MipRange(0, 1)
								},
								Framegraph::SubpassInputAttachmentReference{
									(Framegraph::AttachmentIndex)PostProcessStage::Images::TemporalAAHistory,
									MipRange(0, 1)
								},
								Framegraph::SubpassInputAttachmentReference{
									(Framegraph::AttachmentIndex)PostProcessStage::Images::TemporalAAVelocity,
									MipRange(0, 1)
								}
							}),
						},
#endif
#if ENABLE_FSR
						Framegraph::ComputeSubpassDescription{
							"Super Resolution",
							framegraphBuilder.EmplaceSubpassAttachmentReferences(Array{Framegraph::SubpassOutputAttachmentReference{
								(Framegraph::AttachmentIndex)PostProcessStage::Images::SuperResolution,
								MipRange(0, 1)
							}}),
							ArrayView<const Framegraph::SubpassInputOutputAttachmentReference, Framegraph::AttachmentIndex>{},
							framegraphBuilder.EmplaceSubpassAttachmentReferences(Array{Framegraph::SubpassInputAttachmentReference{
								(Framegraph::AttachmentIndex)PostProcessStage::Images::Composite,
								MipRange(0, 1)
							}}),
						},
						Framegraph::ComputeSubpassDescription{
							"Sharpen",
							framegraphBuilder.EmplaceSubpassAttachmentReferences(Array{Framegraph::SubpassOutputAttachmentReference{
								(Framegraph::AttachmentIndex)PostProcessStage::Images::RenderOutput,
								MipRange(0, 1)
							}}),
							ArrayView<const Framegraph::SubpassInputOutputAttachmentReference, Framegraph::AttachmentIndex>{},
							framegraphBuilder.EmplaceSubpassAttachmentReferences(Array{Framegraph::SubpassInputAttachmentReference{
								(Framegraph::AttachmentIndex)PostProcessStage::Images::SuperResolution,
								MipRange(0, 1)
							}}),
						},
#endif
				})
			});

			if constexpr (ENABLE_TAA && !ENABLE_FSR)
			{
				framegraphBuilder.EmplaceStage(Framegraph::RenderPassStageDescription{
					"copy HDR to screen",
					Framegraph::InvalidStageIndex,
					*m_pCopyToScreenStage,
					sceneViewDrawer,
					outputArea,
					framegraphBuilder.EmplaceColorAttachments(Array{
						Framegraph::ColorAttachmentDescription{
							renderOutputRenderTargetTemplateIdentifier,
							outputArea.GetSize(),
							MipRange(0, 1),
							ArrayRange(0, 1),
							Framegraph::AttachmentFlags::CanRead | Framegraph::AttachmentFlags::CanStore,
							Optional<Math::Color>{}
						},
					}),
					Optional<Framegraph::DepthAttachmentDescription>{},
					Optional<Framegraph::StencilAttachmentDescription>{},
					framegraphBuilder.EmplaceInputAttachments(Array{Framegraph::InputAttachmentDescription{
						textureCache.FindOrRegisterRenderTargetTemplate(PBRLightingStage::HDRSceneRenderTargetAssetGuid),
						outputArea.GetSize(),
						ImageSubresourceRange{ImageAspectFlags::Color, MipRange(0, 1), ArrayRange(0, 1)}
					}})
				});

				/*framegraphBuilder.EmplaceStage(Framegraph::RenderPassStageDescription{
				  "copy TAA history",
				  Framegraph::InvalidStageIndex,
				  *m_pCopyTemporalAAHistoryStage,
				  sceneViewDrawer,
				  outputArea,
				  framegraphBuilder.EmplaceColorAttachments(Array{
				    Framegraph::ColorAttachmentDescription{
				      textureCache.FindOrRegisterRenderTargetTemplate(PostProcessStage::TAAHistorySceneRenderTargetAssetGuid),
				      outputArea.GetSize(),
				      MipRange(0, 1),
				      ArrayRange(0, 1),
				      "#000000"_color
				    },
				  }),
				  Optional<Framegraph::DepthAttachmentDescription>{},
				  Optional<Framegraph::StencilAttachmentDescription>{},
				  framegraphBuilder.EmplaceInputAttachments(Array{Framegraph::InputAttachmentDescription{
				    textureCache.FindOrRegisterRenderTargetTemplate(PBRLightingStage::HDRSceneRenderTargetAssetGuid),
				    outputArea.GetSize(),
				    ImageSubresourceRange{ImageAspectFlags::Color, MipRange(0, 1), ArrayRange(0, 1)}
				  }})
				});*/
			}
		}
	}

	SceneFramegraph::SceneFramegraph(LogicalDevice& logicalDevice, RenderOutput& renderOutput)
		: Framegraph(logicalDevice, renderOutput)
	{
	}
	SceneFramegraph::~SceneFramegraph() = default;

	void SceneFramegraph::Build(Rendering::SceneView& sceneView, Threading::JobBatch& jobBatchOut)
	{
		UniquePtr<FramegraphBuilder> pFramegraphBuilder{Memory::ConstructInPlace};
		FramegraphBuilder& framegraphBuilder = *pFramegraphBuilder;
		SceneFramegraphBuilder::Build(framegraphBuilder, sceneView);
		Framegraph::Compile(framegraphBuilder.GetStages(), jobBatchOut);
	}
}
