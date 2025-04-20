#pragma once

#include <Common/Math/ForwardDeclarations/Vector2.h>
#include <Common/Math/Primitives/ForwardDeclarations/Rectangle.h>
#include <Common/Math/Matrix3x3.h>
#include <Common/Math/Matrix4x4.h>
#include <Common/Math/WorldCoordinate.h>

#include <Renderer/Pipelines/GraphicsPipeline.h>
#include <Renderer/Descriptors/DescriptorSetLayout.h>

#include <DeferredShading/Features.h>

namespace ngine::Rendering
{
	struct LogicalDeviceView;
	struct RenderCommandEncoderView;
	struct ShaderCache;
	struct TextureCache;
	struct MeshCache;
	struct RenderPassView;
	struct RenderMeshView;
	struct TransformBufferView;
	struct ViewMatrices;

	struct InstanceGroupPerLogicalDeviceData;

	struct PBRLightingPipeline final : public DescriptorSetLayout, public GraphicsPipeline
	{
		enum class DescriptorBinding : uint8
		{
			First,
			BRDFTexture = First,
			BRDFSampler,
			TilesTexture,
			TilesSampler,

#define ENABLE_DEFERRED_LIGHTING_SUBPASSES (!RENDERER_WEBGPU)
#if ENABLE_DEFERRED_LIGHTING_SUBPASSES
			AlbedoTexture,
			NormalsTexture,
			MaterialPropertiesTexture,
			DepthTexture,
#else
			AlbedoTexture,
			AlbedoSampler,
			NormalsTexture,
			NormalsSampler,
			MaterialPropertiesTexture,
			MaterialPropertiesSampler,
			DepthTexture,
#endif

			ShadowMapArrayTexture,
			ShadowMapArraySampler,
			IrradianceTexture,
			IrradianceSampler,
			PrefilteredMapTexture,
			PrefilteredMapSampler,
			PointLightsBuffer,
			SpotLightsBuffer,
			DirectionalLightsBuffer,
#if ENABLE_SAMPLE_DISTRIBUTION_SHADOW_MAPS
			ShadowInfoBuffer,
#endif
			Count
		};

		PBRLightingPipeline(
			Rendering::LogicalDevice& logicalDevice,
			const DescriptorSetLayoutView viewInfoDescriptorSet,
			TextureCache& textureCache,
			MeshCache& meshCache
		);
		PBRLightingPipeline(const PBRLightingPipeline&) = delete;
		PBRLightingPipeline& operator=(const PBRLightingPipeline&) = delete;
		PBRLightingPipeline(PBRLightingPipeline&& other) = default;
		PBRLightingPipeline& operator=(PBRLightingPipeline&&) = delete;

		[[nodiscard]] bool IsValid() const
		{
			return GraphicsPipeline::IsValid() & DescriptorSetLayout::IsValid();
		}

		void Destroy(LogicalDevice& logicalDevice);
		[[nodiscard]] Threading::JobBatch CreatePipeline(
			LogicalDevice& logicalDevice,
			ShaderCache& shaderCache,
			const RenderPassView renderPass,
			const Math::Rectangleui outputArea,
			const Math::Rectangleui renderArea,
			const uint8 subpassIndex
		);
	};

	struct PBRLightingPipelineRaytraced final : public DescriptorSetLayout, public GraphicsPipeline
	{
		enum class DescriptorBinding : uint8
		{
			First,
			BRDFTexture = First,
			BRDFSampler,
			TilesTexture,
			TilesSampler,

#define ENABLE_DEFERRED_LIGHTING_SUBPASSES (!RENDERER_WEBGPU)
#if ENABLE_DEFERRED_LIGHTING_SUBPASSES
			AlbedoTexture,
			NormalsTexture,
			MaterialPropertiesTexture,
			DepthTexture,
#else
			AlbedoTexture,
			AlbedoSampler,
			NormalsTexture,
			NormalsSampler,
			MaterialPropertiesTexture,
			MaterialPropertiesSampler,
			DepthTexture,
#endif

			AccelerationStructure,
			IrradianceTexture,
			IrradianceSampler,
			PrefilteredMapTexture,
			PrefilteredMapSampler,
			PointLightsBuffer,
			SpotLightsBuffer,
			DirectionalLightsBuffer,
			RenderItemsBuffer,
			Count
		};

		PBRLightingPipelineRaytraced();
		PBRLightingPipelineRaytraced(
			Rendering::LogicalDevice& logicalDevice,
			const DescriptorSetLayoutView viewInfoDescriptorSet,
			TextureCache& textureCache,
			MeshCache& meshCache
		);
		PBRLightingPipelineRaytraced(const PBRLightingPipelineRaytraced&) = delete;
		PBRLightingPipelineRaytraced& operator=(const PBRLightingPipelineRaytraced&) = delete;
		PBRLightingPipelineRaytraced(PBRLightingPipelineRaytraced&& other) = default;
		PBRLightingPipelineRaytraced& operator=(PBRLightingPipelineRaytraced&&) = delete;

		[[nodiscard]] bool IsValid() const
		{
			return GraphicsPipeline::IsValid() & DescriptorSetLayout::IsValid();
		}

		void Destroy(LogicalDevice& logicalDevice);
		[[nodiscard]] Threading::JobBatch CreatePipeline(
			LogicalDevice& logicalDevice,
			ShaderCache& shaderCache,
			const RenderPassView renderPass,
			const Math::Rectangleui outputArea,
			const Math::Rectangleui renderArea,
			const uint8 subpassIndex
		);
	};
}
