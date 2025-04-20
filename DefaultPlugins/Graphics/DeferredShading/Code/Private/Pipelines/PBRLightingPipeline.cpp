#include "Pipelines/PBRLightingPipeline.h"
#include "Features.h"

#include <Renderer/Vulkan/Includes.h>
#include <Renderer/Wrappers/RenderPassView.h>
#include <Renderer/Assets/StaticMesh/RenderMeshView.h>
#include <Renderer/Assets/StaticMesh/MeshCache.h>
#include <Renderer/Assets/Shader/ShaderCache.h>
#include <Renderer/Assets/Texture/TextureCache.h>
#include <Renderer/Scene/ViewMatrices.h>
#include <Renderer/Devices/LogicalDevice.h>
#include <Renderer/Devices/PhysicalDevice.h>
#include <Renderer/Pipelines/PushConstantRange.h>

#include <Common/Math/Color.h>
#include <Common/Math/Primitives/Rectangle.h>
#include <Common/Math/Matrix4x4.h>
#include <Common/Math/Vector2.h>
#include <Common/Memory/OffsetOf.h>
#include <Common/Memory/Containers/FlatVector.h>
#include <Common/Threading/Jobs/JobBatch.h>

#include <DeferredShading/FSR/fsr_settings.h>

namespace ngine::Rendering
{
	inline static constexpr Array<const DescriptorSetLayout::Binding, (uint8)PBRLightingPipeline::DescriptorBinding::Count>
		LightingDescriptorBindings
	{
		DescriptorSetLayout::Binding::MakeSampledImage(
			(uint8)PBRLightingPipeline::DescriptorBinding::BRDFTexture,
			ShaderStage::Fragment,
			SampledImageType::Float,
			ImageMappingType::TwoDimensional
		),
			DescriptorSetLayout::Binding::MakeSampler(
				(uint8)PBRLightingPipeline::DescriptorBinding::BRDFSampler,
				ShaderStage::Fragment,
				SamplerBindingType::Filtering
			),
			DescriptorSetLayout::Binding::MakeSampledImage(
				(uint8)PBRLightingPipeline::DescriptorBinding::TilesTexture,
				ShaderStage::Fragment,
				SampledImageType::UnsignedInteger,
				ImageMappingType::TwoDimensional
			),
			DescriptorSetLayout::Binding::MakeSampler(
				(uint8)PBRLightingPipeline::DescriptorBinding::TilesSampler,
				ShaderStage::Fragment,
				SamplerBindingType::Filtering
			),

#if ENABLE_DEFERRED_LIGHTING_SUBPASSES
			DescriptorSetLayout::Binding::MakeInputAttachment(
				(uint8)PBRLightingPipeline::DescriptorBinding::AlbedoTexture,
				ShaderStage::Fragment,
				SampledImageType::Float,
				ImageMappingType::TwoDimensional
			),
			DescriptorSetLayout::Binding::MakeInputAttachment(
				(uint8)PBRLightingPipeline::DescriptorBinding::NormalsTexture,
				ShaderStage::Fragment,
				SampledImageType::Float,
				ImageMappingType::TwoDimensional
			),
			DescriptorSetLayout::Binding::MakeInputAttachment(
				(uint8)PBRLightingPipeline::DescriptorBinding::MaterialPropertiesTexture,
				ShaderStage::Fragment,
				SampledImageType::Float,
				ImageMappingType::TwoDimensional
			),
			DescriptorSetLayout::Binding::MakeInputAttachment(
				(uint8)PBRLightingPipeline::DescriptorBinding::DepthTexture,
				ShaderStage::Fragment,
				SampledImageType::Depth,
				ImageMappingType::TwoDimensional
			),
#else
			DescriptorSetLayout::Binding::MakeSampledImage(
				(uint8)PBRLightingPipeline::DescriptorBinding::AlbedoTexture,
				ShaderStage::Fragment,
				SampledImageType::Float,
				ImageMappingType::TwoDimensional
			),
			DescriptorSetLayout::Binding::MakeSampler(
				(uint8)PBRLightingPipeline::DescriptorBinding::AlbedoSampler,
				ShaderStage::Fragment,
				SamplerBindingType::Filtering
			),
			DescriptorSetLayout::Binding::MakeSampledImage(
				(uint8)PBRLightingPipeline::DescriptorBinding::NormalsTexture,
				ShaderStage::Fragment,
				SampledImageType::Float,
				ImageMappingType::TwoDimensional
			),
			DescriptorSetLayout::Binding::MakeSampler(
				(uint8)PBRLightingPipeline::DescriptorBinding::NormalsSampler,
				ShaderStage::Fragment,
				SamplerBindingType::Filtering
			),
			DescriptorSetLayout::Binding::MakeSampledImage(
				(uint8)PBRLightingPipeline::DescriptorBinding::MaterialPropertiesTexture,
				ShaderStage::Fragment,
				SampledImageType::Float,
				ImageMappingType::TwoDimensional
			),
			DescriptorSetLayout::Binding::MakeSampler(
				(uint8)PBRLightingPipeline::DescriptorBinding::MaterialPropertiesSampler,
				ShaderStage::Fragment,
				SamplerBindingType::Filtering
			),
			DescriptorSetLayout::Binding::MakeSampledImage(
				(uint8)PBRLightingPipeline::DescriptorBinding::DepthTexture,
				ShaderStage::Fragment,
				SampledImageType::UnfilterableFloat,
				ImageMappingType::TwoDimensional
			),
#endif

			DescriptorSetLayout::Binding::MakeSampledImage(
				(uint8)PBRLightingPipeline::DescriptorBinding::ShadowMapArrayTexture,
				ShaderStage::Fragment,
				SampledImageType::Depth,
				ImageMappingType::TwoDimensionalArray
			),
			DescriptorSetLayout::Binding::MakeSampler(
				(uint8)PBRLightingPipeline::DescriptorBinding::ShadowMapArraySampler,
				ShaderStage::Fragment,
				SamplerBindingType::Comparison
			),
			DescriptorSetLayout::Binding::MakeSampledImage(
				(uint8)PBRLightingPipeline::DescriptorBinding::IrradianceTexture,
				ShaderStage::Fragment,
				SampledImageType::Float,
				ImageMappingType::Cube
			),
			DescriptorSetLayout::Binding::MakeSampler(
				(uint8)PBRLightingPipeline::DescriptorBinding::IrradianceSampler,
				ShaderStage::Fragment,
				SamplerBindingType::Filtering
			),
			DescriptorSetLayout::Binding::MakeSampledImage(
				(uint8)PBRLightingPipeline::DescriptorBinding::PrefilteredMapTexture,
				ShaderStage::Fragment,
				SampledImageType::Float,
				ImageMappingType::Cube
			),
			DescriptorSetLayout::Binding::MakeSampler(
				(uint8)PBRLightingPipeline::DescriptorBinding::PrefilteredMapSampler,
				ShaderStage::Fragment,
				SamplerBindingType::Filtering
			),
			DescriptorSetLayout::Binding::MakeStorageBuffer(
				(uint8)PBRLightingPipeline::DescriptorBinding::PointLightsBuffer,
				ShaderStage::Fragment
			),
			DescriptorSetLayout::Binding::MakeStorageBuffer(
				(uint8)PBRLightingPipeline::DescriptorBinding::SpotLightsBuffer,
				ShaderStage::Fragment
			),
			DescriptorSetLayout::Binding::MakeStorageBuffer(
				(uint8)PBRLightingPipeline::DescriptorBinding::DirectionalLightsBuffer,
				ShaderStage::Fragment
			),
#if ENABLE_SAMPLE_DISTRIBUTION_SHADOW_MAPS
			DescriptorSetLayout::Binding::MakeStorageBuffer(
				(uint8)PBRLightingPipeline::DescriptorBinding::ShadowInfoBuffer,
				ShaderStage::Fragment
			),
#endif
	};

	inline static constexpr Array<const DescriptorSetLayout::Binding, (uint8)PBRLightingPipelineRaytraced::DescriptorBinding::Count>
		LightingDescriptorBindingsRaytraced
	{
		DescriptorSetLayout::Binding::MakeSampledImage(
			(uint8)PBRLightingPipelineRaytraced::DescriptorBinding::BRDFTexture,
			ShaderStage::Fragment,
			SampledImageType::Float,
			ImageMappingType::TwoDimensional
		),
			DescriptorSetLayout::Binding::MakeSampler(
				(uint8)PBRLightingPipelineRaytraced::DescriptorBinding::BRDFSampler,
				ShaderStage::Fragment,
				SamplerBindingType::Filtering
			),
			DescriptorSetLayout::Binding::MakeSampledImage(
				(uint8)PBRLightingPipelineRaytraced::DescriptorBinding::TilesTexture,
				ShaderStage::Fragment,
				SampledImageType::UnsignedInteger,
				ImageMappingType::TwoDimensional
			),
			DescriptorSetLayout::Binding::MakeSampler(
				(uint8)PBRLightingPipelineRaytraced::DescriptorBinding::TilesSampler,
				ShaderStage::Fragment,
				SamplerBindingType::Filtering
			),

#if ENABLE_DEFERRED_LIGHTING_SUBPASSES
			DescriptorSetLayout::Binding::MakeInputAttachment(
				(uint8)PBRLightingPipelineRaytraced::DescriptorBinding::AlbedoTexture,
				ShaderStage::Fragment,
				SampledImageType::Float,
				ImageMappingType::TwoDimensional
			),
			DescriptorSetLayout::Binding::MakeInputAttachment(
				(uint8)PBRLightingPipelineRaytraced::DescriptorBinding::NormalsTexture,
				ShaderStage::Fragment,
				SampledImageType::Float,
				ImageMappingType::TwoDimensional
			),
			DescriptorSetLayout::Binding::MakeInputAttachment(
				(uint8)PBRLightingPipelineRaytraced::DescriptorBinding::MaterialPropertiesTexture,
				ShaderStage::Fragment,
				SampledImageType::Float,
				ImageMappingType::TwoDimensional
			),
			DescriptorSetLayout::Binding::MakeInputAttachment(
				(uint8)PBRLightingPipelineRaytraced::DescriptorBinding::DepthTexture,
				ShaderStage::Fragment,
				SampledImageType::Depth,
				ImageMappingType::TwoDimensional
			),
#else
			DescriptorSetLayout::Binding::MakeSampledImage(
				(uint8)PBRLightingPipelineRaytraced::DescriptorBinding::AlbedoTexture,
				ShaderStage::Fragment,
				SampledImageType::Float,
				ImageMappingType::TwoDimensional
			),
			DescriptorSetLayout::Binding::MakeSampler(
				(uint8)PBRLightingPipelineRaytraced::DescriptorBinding::AlbedoSampler,
				ShaderStage::Fragment,
				SamplerBindingType::Filtering
			),
			DescriptorSetLayout::Binding::MakeSampledImage(
				(uint8)PBRLightingPipelineRaytraced::DescriptorBinding::NormalsTexture,
				ShaderStage::Fragment,
				SampledImageType::Float,
				ImageMappingType::TwoDimensional
			),
			DescriptorSetLayout::Binding::MakeSampler(
				(uint8)PBRLightingPipelineRaytraced::DescriptorBinding::NormalsSampler,
				ShaderStage::Fragment,
				SamplerBindingType::Filtering
			),
			DescriptorSetLayout::Binding::MakeSampledImage(
				(uint8)PBRLightingPipelineRaytraced::DescriptorBinding::MaterialPropertiesTexture,
				ShaderStage::Fragment,
				SampledImageType::Float,
				ImageMappingType::TwoDimensional
			),
			DescriptorSetLayout::Binding::MakeSampler(
				(uint8)PBRLightingPipelineRaytraced::DescriptorBinding::MaterialPropertiesSampler,
				ShaderStage::Fragment,
				SamplerBindingType::Filtering
			),
			DescriptorSetLayout::Binding::MakeSampledImage(
				(uint8)PBRLightingPipelineRaytraced::DescriptorBinding::DepthTexture,
				ShaderStage::Fragment,
				SampledImageType::UnfilterableFloat,
				ImageMappingType::TwoDimensional
			),
#endif

			DescriptorSetLayout::Binding::MakeAccelerationStructure(
				(uint8)PBRLightingPipelineRaytraced::DescriptorBinding::AccelerationStructure,
				ShaderStage::Fragment
			),
			DescriptorSetLayout::Binding::MakeSampledImage(
				(uint8)PBRLightingPipelineRaytraced::DescriptorBinding::IrradianceTexture,
				ShaderStage::Fragment,
				SampledImageType::Float,
				ImageMappingType::Cube
			),
			DescriptorSetLayout::Binding::MakeSampler(
				(uint8)PBRLightingPipelineRaytraced::DescriptorBinding::IrradianceSampler,
				ShaderStage::Fragment,
				SamplerBindingType::Filtering
			),
			DescriptorSetLayout::Binding::MakeSampledImage(
				(uint8)PBRLightingPipelineRaytraced::DescriptorBinding::PrefilteredMapTexture,
				ShaderStage::Fragment,
				SampledImageType::Float,
				ImageMappingType::Cube
			),
			DescriptorSetLayout::Binding::MakeSampler(
				(uint8)PBRLightingPipelineRaytraced::DescriptorBinding::PrefilteredMapSampler,
				ShaderStage::Fragment,
				SamplerBindingType::Filtering
			),
			DescriptorSetLayout::Binding::MakeStorageBuffer(
				(uint8)PBRLightingPipelineRaytraced::DescriptorBinding::PointLightsBuffer,
				ShaderStage::Fragment
			),
			DescriptorSetLayout::Binding::MakeStorageBuffer(
				(uint8)PBRLightingPipelineRaytraced::DescriptorBinding::SpotLightsBuffer,
				ShaderStage::Fragment
			),
			DescriptorSetLayout::Binding::MakeStorageBuffer(
				(uint8)PBRLightingPipelineRaytraced::DescriptorBinding::DirectionalLightsBuffer,
				ShaderStage::Fragment
			),
			DescriptorSetLayout::Binding::MakeStorageBuffer(
				(uint8)PBRLightingPipelineRaytraced::DescriptorBinding::RenderItemsBuffer,
				ShaderStage::Fragment
			), //  render items (mesh, material info etc)
	};

	[[nodiscard]] FlatVector<DescriptorSetLayoutView, 4> GetDescriptorSetLayouts(
		Rendering::LogicalDevice& logicalDevice,
		const DescriptorSetLayoutView lightingDescriptorSet,
		const DescriptorSetLayoutView viewInfoDescriptorSet,
		TextureCache& textureCache,
		MeshCache& meshCache
	)
	{
		FlatVector<DescriptorSetLayoutView, 4> descriptorSets{lightingDescriptorSet, viewInfoDescriptorSet};
		const DescriptorSetLayoutView texturesDescriptorSetLayout = textureCache.GetTexturesDescriptorSetLayout(logicalDevice.GetIdentifier());
		if (texturesDescriptorSetLayout.IsValid())
		{
			descriptorSets.EmplaceBack(texturesDescriptorSetLayout);
		}

		const DescriptorSetLayoutView meshesDescriptorSetLayout = meshCache.GetMeshesDescriptorSetLayout(logicalDevice.GetIdentifier());
		if (meshesDescriptorSetLayout.IsValid())
		{
			descriptorSets.EmplaceBack(meshesDescriptorSetLayout);
		}

		return descriptorSets;
	}

	PBRLightingPipeline::PBRLightingPipeline(
		Rendering::LogicalDevice& logicalDevice,
		const DescriptorSetLayoutView viewInfoDescriptorSet,
		TextureCache& textureCache,
		MeshCache& meshCache
	)
		: DescriptorSetLayout(logicalDevice, LightingDescriptorBindings)
	{
#if RENDERER_OBJECT_DEBUG_NAMES
		DescriptorSetLayout::SetDebugName(logicalDevice, "PBR Lighting");
#endif
		CreateBase(logicalDevice, GetDescriptorSetLayouts(logicalDevice, *this, viewInfoDescriptorSet, textureCache, meshCache), {});
	}

	void PBRLightingPipeline::Destroy(LogicalDevice& logicalDevice)
	{
		GraphicsPipeline::Destroy(logicalDevice);
		DescriptorSetLayout::Destroy(logicalDevice);
	}

	Threading::JobBatch PBRLightingPipeline::CreatePipeline(
		LogicalDevice& logicalDevice,
		ShaderCache& shaderCache,
		const RenderPassView renderPass,
		[[maybe_unused]] const Math::Rectangleui outputArea,
		const Math::Rectangleui renderArea,
		const uint8 subpassIndex
	)
	{
		const VertexStageInfo vertexStage{ShaderStageInfo{"0fb14713-0658-3759-eae2-d5da71cbee9c"_asset}};

		const PrimitiveInfo primitiveInfo{PrimitiveTopology::TriangleList, PolygonMode::Fill, WindingOrder::CounterClockwise, CullMode::Front};

		const Array<Viewport, 1> viewports{Viewport{renderArea}};
		const Array<Math::Rectangleui, 1> scissors{renderArea};

		const Array<ColorTargetInfo, 1> colorBlendAttachments{ColorTargetInfo{}};

		const FragmentStageInfo fragmentStage{ShaderStageInfo{"08060b77-500a-8918-345f-a639a786c6cd"_asset}, colorBlendAttachments};

		EnumFlags<DynamicStateFlags> dynamicStateFlags{};
		// TODO: Hardwired for VR / XR, change to pass in a flag instead
		// dynamicStateFlags |= DynamicStateFlags::Viewport * (outputArea != renderArea); // Might need to comment out for FSR changes

		return CreateAsync(
			logicalDevice,
			shaderCache,
			m_pipelineLayout,
			renderPass,
			vertexStage,
			primitiveInfo,
			viewports,
			scissors,
			subpassIndex,
			fragmentStage,
			Optional<const MultisamplingInfo*>{},
			Optional<const DepthStencilInfo*>{},
			Optional<const GeometryStageInfo*>{},
			dynamicStateFlags
		);
	}

	PBRLightingPipelineRaytraced::PBRLightingPipelineRaytraced() = default;

	PBRLightingPipelineRaytraced::PBRLightingPipelineRaytraced(
		Rendering::LogicalDevice& logicalDevice,
		const DescriptorSetLayoutView viewInfoDescriptorSet,
		TextureCache& textureCache,
		MeshCache& meshCache
	)
		: DescriptorSetLayout(logicalDevice, LightingDescriptorBindingsRaytraced)
	{
#if RENDERER_OBJECT_DEBUG_NAMES
		DescriptorSetLayout::SetDebugName(logicalDevice, "PBR Lighting");
#endif
		CreateBase(logicalDevice, GetDescriptorSetLayouts(logicalDevice, *this, viewInfoDescriptorSet, textureCache, meshCache), {});
	}

	void PBRLightingPipelineRaytraced::Destroy(LogicalDevice& logicalDevice)
	{
		GraphicsPipeline::Destroy(logicalDevice);
		DescriptorSetLayout::Destroy(logicalDevice);
	}

	Threading::JobBatch PBRLightingPipelineRaytraced::CreatePipeline(
		LogicalDevice& logicalDevice,
		ShaderCache& shaderCache,
		const RenderPassView renderPass,
		[[maybe_unused]] const Math::Rectangleui outputArea,
		const Math::Rectangleui renderArea,
		const uint8 subpassIndex
	)
	{
		const VertexStageInfo vertexStage{ShaderStageInfo{"0fb14713-0658-3759-eae2-d5da71cbee9c"_asset}};

		const PrimitiveInfo primitiveInfo{PrimitiveTopology::TriangleList, PolygonMode::Fill, WindingOrder::CounterClockwise, CullMode::Front};

		const Array<Viewport, 1> viewports{Viewport{renderArea}};
		const Array<Math::Rectangleui, 1> scissors{renderArea};

		const Array<ColorTargetInfo, 1> colorBlendAttachments{ColorTargetInfo{}};

		const FragmentStageInfo fragmentStage{ShaderStageInfo{"32c84a02-43f7-4236-93a2-012e320eb4a4"_asset}, colorBlendAttachments};

		EnumFlags<DynamicStateFlags> dynamicStateFlags{};
		// TODO: Hardwired for VR / XR, change to pass in a flag instead
		// dynamicStateFlags |= DynamicStateFlags::Viewport * (outputArea != renderArea); // Might need to comment out for FSR changes

		return CreateAsync(
			logicalDevice,
			shaderCache,
			m_pipelineLayout,
			renderPass,
			vertexStage,
			primitiveInfo,
			viewports,
			scissors,
			subpassIndex,
			fragmentStage,
			Optional<const MultisamplingInfo*>{},
			Optional<const DepthStencilInfo*>{},
			Optional<const GeometryStageInfo*>{},
			dynamicStateFlags
		);
	}
}
