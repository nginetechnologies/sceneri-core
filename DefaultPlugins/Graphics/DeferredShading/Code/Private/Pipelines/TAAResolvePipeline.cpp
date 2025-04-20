#include "Pipelines/TAAResolvePipeline.h"

#include <Common/Math/Color.h>
#include <Common/Math/Primitives/Rectangle.h>
#include <Common/Math/Matrix4x4.h>
#include <Common/Memory/OffsetOf.h>

#include <Renderer/Vulkan/Includes.h>
#include <Renderer/Wrappers/RenderPassView.h>
#include <Renderer/Assets/StaticMesh/RenderMeshView.h>
#include <Renderer/Pipelines/PushConstantRange.h>

#include <Renderer/Assets/Shader/ShaderCache.h>
#include <Renderer/Scene/ViewMatrices.h>
#include <Renderer/Devices/LogicalDevice.h>

namespace ngine::Rendering
{
	struct PostProcessConstants
	{
		struct ComputeConstants
		{
			Math::Vector4f m_viewRatio;
		};

		ComputeConstants computeConstants;
	};

	inline static constexpr Array<const PushConstantRange, 1> PostProcessPushConstantRanges = {PushConstantRange{
		ShaderStage::Compute, OFFSET_OF(PostProcessConstants, computeConstants), sizeof(PostProcessConstants::ComputeConstants)
	}};

	inline static constexpr Array<const DescriptorSetLayout::Binding, 6> TAAResolveDescriptorBindings = {
		DescriptorSetLayout::Binding::MakeSampledImage(
			0, ShaderStage::Compute, SampledImageType::Float, ImageMappingType::TwoDimensional
		), // Composite color
		DescriptorSetLayout::Binding::MakeSampledImage(
			1, ShaderStage::Compute, SampledImageType::Float, ImageMappingType::TwoDimensional
		), // History buffer
		DescriptorSetLayout::Binding::MakeStorageImage(
			2, ShaderStage::Compute, Format::R16G16B16A16_SFLOAT, StorageTextureAccess::WriteOnly
		),                                                                                                 // Intermediate buffer
		DescriptorSetLayout::Binding::MakeSampler(3, ShaderStage::Compute, SamplerBindingType::Filtering), // Nearest sampler (Composite)
		DescriptorSetLayout::Binding::MakeSampler(4, ShaderStage::Compute, SamplerBindingType::Filtering), // Linear sampler  (History)
		DescriptorSetLayout::Binding::MakeSampledImage(5, ShaderStage::Compute, SampledImageType::Float, ImageMappingType::TwoDimensional)
	}; // Velocity buffer

	TAAResolvePipeline::TAAResolvePipeline(Rendering::LogicalDevice& logicalDevice, ShaderCache& shaderCache)
		: DescriptorSetLayout(logicalDevice, TAAResolveDescriptorBindings)
	{
#if RENDERER_OBJECT_DEBUG_NAMES
		DescriptorSetLayout::SetDebugName(logicalDevice, "TAA Resolve");
#endif
		CreateBase(logicalDevice, ArrayView<const DescriptorSetLayoutView>{*this}, PostProcessPushConstantRanges);
		Create(logicalDevice, shaderCache, ShaderStageInfo{"7ce18564-7e31-4b58-8701-21b9c32079a3"_asset}, m_pipelineLayout);
	}

	void TAAResolvePipeline::Destroy(LogicalDevice& logicalDevice)
	{
		ComputePipeline::Destroy(logicalDevice);
		DescriptorSetLayout::Destroy(logicalDevice);
	}

	void TAAResolvePipeline::Compute(
		Rendering::LogicalDevice& logicalDevice,
		ArrayView<const DescriptorSetView, uint8> imageDescriptorSets,
		const ComputeCommandEncoderView computeCommandEncoder,
		const Math::Vector2ui resolution,
		const Math::Vector4f viewRatio
	) const
	{
		PostProcessConstants pushConstants{PostProcessConstants::ComputeConstants{viewRatio}};

		PushConstants(logicalDevice, computeCommandEncoder, PostProcessPushConstantRanges, pushConstants);

		computeCommandEncoder.BindDescriptorSets(m_pipelineLayout, imageDescriptorSets, GetFirstDescriptorSetIndex());

		const Math::Vector3ui threadGroupSize{8, 8, 1};
		computeCommandEncoder.Dispatch(GetNumberOfThreadGroups(resolution, threadGroupSize), Math::Vector3ui{8, 8, 1});
	}
}
