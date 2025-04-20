#include "Pipelines/DepthMinMaxPyramidPipelines.h"

#include <Renderer/Vulkan/Includes.h>

#include <Renderer/Assets/Shader/ShaderCache.h>
#include <Renderer/Devices/LogicalDevice.h>
#include <Renderer/Pipelines/PushConstantRange.h>

#include <Common/Math/Vector2.h>
#include <Common/Math/Vector3.h>

namespace ngine::Rendering
{
	inline static constexpr Array<const DescriptorSetLayout::Binding, 2> InitialDepthMinMaxPyramidDescriptorBindings = {
		DescriptorSetLayout::Binding::MakeSampledImage(
			0, ShaderStage::Compute, SampledImageType::UnfilterableFloat, ImageMappingType::TwoDimensional
		),
		DescriptorSetLayout::Binding::MakeStorageImage(1, ShaderStage::Compute, Format::R32G32_SFLOAT, StorageTextureAccess::WriteOnly)
	};

	InitialDepthMinMaxPyramidPipeline::InitialDepthMinMaxPyramidPipeline(Rendering::LogicalDevice& logicalDevice, ShaderCache& shaderCache)
		: DescriptorSetLayout(logicalDevice, InitialDepthMinMaxPyramidDescriptorBindings)
	{
#if RENDERER_OBJECT_DEBUG_NAMES
		DescriptorSetLayout::SetDebugName(logicalDevice, "Initial depth min max");
#endif
		CreateBase(logicalDevice, ArrayView<const DescriptorSetLayoutView>{*this}, {});
		Create(logicalDevice, shaderCache, ShaderStageInfo{"8E51B141-C4ED-4C71-860D-173B55C5AAAC"_asset}, m_pipelineLayout);
	}

	void InitialDepthMinMaxPyramidPipeline::Destroy(LogicalDevice& logicalDevice)
	{
		ComputePipeline::Destroy(logicalDevice);
		DescriptorSetLayout::Destroy(logicalDevice);
	}

	void InitialDepthMinMaxPyramidPipeline::Compute(
		ArrayView<const DescriptorSetView, uint8> imageDescriptorSets,
		const ComputeCommandEncoderView computeCommandEncoder,
		const Math::Vector2ui resolution
	) const
	{
		computeCommandEncoder.BindDescriptorSets(m_pipelineLayout, imageDescriptorSets, GetFirstDescriptorSetIndex());

		const Math::Vector3ui threadGroupSize{8, 8, 1};
		computeCommandEncoder.Dispatch(GetNumberOfThreadGroups(resolution, threadGroupSize), Math::Vector3ui{8, 8, 1});
	}

	//==============================================================================

	inline static constexpr Array<const DescriptorSetLayout::Binding, 2> DepthMinMaxPyramidDescriptorBindings = {
		DescriptorSetLayout::Binding::MakeSampledImage(
			0, ShaderStage::Compute, SampledImageType::UnfilterableFloat, ImageMappingType::TwoDimensional
		),
		DescriptorSetLayout::Binding::MakeStorageImage(1, ShaderStage::Compute, Format::R32G32_SFLOAT, StorageTextureAccess::WriteOnly)
	};

	DepthMinMaxPyramidPipeline::DepthMinMaxPyramidPipeline(Rendering::LogicalDevice& logicalDevice, ShaderCache& shaderCache)
		: DescriptorSetLayout(logicalDevice, DepthMinMaxPyramidDescriptorBindings)
	{
		CreateBase(logicalDevice, ArrayView<const DescriptorSetLayoutView>{*this}, {});
		Create(logicalDevice, shaderCache, ShaderStageInfo{"A6FD3573-8F8A-4849-BDEF-A61317B1AC44"_asset}, m_pipelineLayout);
	}

	void DepthMinMaxPyramidPipeline::Destroy(LogicalDevice& logicalDevice)
	{
		ComputePipeline::Destroy(logicalDevice);
		DescriptorSetLayout::Destroy(logicalDevice);
	}

	void DepthMinMaxPyramidPipeline::Compute(
		ArrayView<const DescriptorSetView, uint8> imageDescriptorSets,
		const ComputeCommandEncoderView computeCommandEncoder,
		const Math::Vector2ui resolution
	) const
	{
		computeCommandEncoder.BindDescriptorSets(m_pipelineLayout, imageDescriptorSets, GetFirstDescriptorSetIndex());

		const Math::Vector3ui threadGroupSize{8, 8, 1};
		computeCommandEncoder.Dispatch(GetNumberOfThreadGroups(resolution, threadGroupSize), Math::Vector3ui{8, 8, 1});
	}
}
