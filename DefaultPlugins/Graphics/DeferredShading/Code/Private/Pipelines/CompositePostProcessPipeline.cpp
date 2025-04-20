#include "Pipelines/CompositePostProcessPipeline.h"

#include <Common/Math/Color.h>
#include <Common/Math/Primitives/Rectangle.h>
#include <Common/Math/Matrix4x4.h>
#include <Common/Memory/OffsetOf.h>
#include <Common/Threading/Jobs/JobBatch.h>

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

	inline static constexpr Array<const DescriptorSetLayout::Binding, 5> CompositeDescriptorBindings = {
		DescriptorSetLayout::Binding::MakeSampledImage(
			0, ShaderStage::Compute, SampledImageType::UnfilterableFloat, ImageMappingType::TwoDimensional
		),
		DescriptorSetLayout::Binding::MakeSampledImage(1, ShaderStage::Compute, SampledImageType::Float, ImageMappingType::TwoDimensional),
		DescriptorSetLayout::Binding::MakeSampledImage(2, ShaderStage::Compute, SampledImageType::Float, ImageMappingType::TwoDimensional),
		DescriptorSetLayout::Binding::MakeStorageImage(
			3, ShaderStage::Compute, RENDERER_WEBGPU ? Format::B8G8R8A8_UNORM : Format::R16G16B16A16_SFLOAT, StorageTextureAccess::WriteOnly
		),
		DescriptorSetLayout::Binding::MakeSampler(4, ShaderStage::Compute, SamplerBindingType::Filtering)
	};

	CompositePostProcessPipeline::CompositePostProcessPipeline(Rendering::LogicalDevice& logicalDevice, ShaderCache& shaderCache)
		: DescriptorSetLayout(logicalDevice, CompositeDescriptorBindings)
	{
#if RENDERER_OBJECT_DEBUG_NAMES
		DescriptorSetLayout::SetDebugName(logicalDevice, "Composite post process");
#endif
		CreateBase(logicalDevice, ArrayView<const DescriptorSetLayoutView>{*this}, PostProcessPushConstantRanges);
		Create(logicalDevice, shaderCache, ShaderStageInfo{"9728A629-CC11-43BD-93EC-CC9025A720A1"_asset}, m_pipelineLayout);
	}

	void CompositePostProcessPipeline::Destroy(LogicalDevice& logicalDevice)
	{
		ComputePipeline::Destroy(logicalDevice);
		DescriptorSetLayout::Destroy(logicalDevice);
	}

	void CompositePostProcessPipeline::Compute(
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
