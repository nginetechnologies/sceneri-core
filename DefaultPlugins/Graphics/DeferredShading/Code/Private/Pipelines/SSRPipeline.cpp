#include "Pipelines/SSRPipeline.h"

#include <Renderer/Vulkan/Includes.h>

#include <Renderer/Assets/Shader/ShaderCache.h>
#include <Renderer/Devices/LogicalDevice.h>
#include <Renderer/Pipelines/PushConstantRange.h>

namespace ngine::Rendering
{
	struct SSRConstants
	{
		float zThickness;
		float nearPlaneZ;
		float stride;
		float cameraFacingReflectionCutoff;
		float screenBorderFadeDistancePx;
		float maxSteps;
		float maxDistance;
	};

	inline static constexpr Array<const PushConstantRange, 1> SSRPushConstantRanges = {
		PushConstantRange{ShaderStage::Compute, 0, sizeof(SSRConstants)}
	};

	inline static constexpr Array<const DescriptorSetLayout::Binding, 5> SSRDescriptorBindings = {
		DescriptorSetLayout::Binding::MakeSampledImage(
			0, ShaderStage::Compute, SampledImageType::UnfilterableFloat, ImageMappingType::TwoDimensional
		),
		DescriptorSetLayout::Binding::MakeSampledImage(
			1, ShaderStage::Compute, SampledImageType::UnfilterableFloat, ImageMappingType::TwoDimensional
		),
		DescriptorSetLayout::Binding::MakeSampledImage(
			2, ShaderStage::Compute, SampledImageType::UnfilterableFloat, ImageMappingType::TwoDimensional
		),
		DescriptorSetLayout::Binding::MakeSampledImage(
			3, ShaderStage::Compute, SampledImageType::UnfilterableFloat, ImageMappingType::TwoDimensional
		),
		DescriptorSetLayout::Binding::MakeStorageImage(4, ShaderStage::Compute, Format::R16G16B16A16_SFLOAT, StorageTextureAccess::WriteOnly)
	};

	SSRPipeline::SSRPipeline(
		Rendering::LogicalDevice& logicalDevice, ShaderCache& shaderCache, const DescriptorSetLayoutView viewInfoDescriptorSetLayout
	)
		: DescriptorSetLayout(logicalDevice, SSRDescriptorBindings)
	{
#if RENDERER_OBJECT_DEBUG_NAMES
		DescriptorSetLayout::SetDebugName(logicalDevice, "SSR");
#endif
		CreateBase(logicalDevice, Array<const DescriptorSetLayoutView, 2>{*this, viewInfoDescriptorSetLayout}, SSRPushConstantRanges);
		Create(logicalDevice, shaderCache, ShaderStageInfo{"0FA74CF9-9F60-4F70-A9F1-F653C61562B7"_asset}, m_pipelineLayout);
	}

	void SSRPipeline::Destroy(LogicalDevice& logicalDevice)
	{
		ComputePipeline::Destroy(logicalDevice);
		DescriptorSetLayout::Destroy(logicalDevice);
	}

	void SSRPipeline::Compute(
		Rendering::LogicalDevice& logicalDevice,
		float nearPlane,
		ArrayView<const DescriptorSetView, uint8> imageDescriptorSets,
		const ComputeCommandEncoderView computeCommandEncoder,
		const Math::Vector2ui resolution
	) const
	{
		SSRConstants constants;
		constants.zThickness = 0.5f;
		constants.nearPlaneZ = nearPlane;
		constants.cameraFacingReflectionCutoff = 0.9f;
		constants.screenBorderFadeDistancePx = 400.0f;
#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
		constants.stride =
			16.0f; // jittering the ray start pos can somewhat hide the loss of quality due to stride, however jittering is still buggy
		constants.maxSteps = 20.0f;
		constants.maxDistance = 5000.0f;
#else
		constants.stride = 1.0f;
		constants.maxSteps = 3000.0f;
		constants.maxDistance = 10000.0f;
#endif

		PushConstants(logicalDevice, computeCommandEncoder, SSRPushConstantRanges, constants);

		computeCommandEncoder.BindDescriptorSets(m_pipelineLayout, imageDescriptorSets, GetFirstDescriptorSetIndex());

		const Math::Vector3ui threadGroupSize{8, 8, 1};
		computeCommandEncoder.Dispatch(GetNumberOfThreadGroups(resolution, threadGroupSize), threadGroupSize);
	}
}
