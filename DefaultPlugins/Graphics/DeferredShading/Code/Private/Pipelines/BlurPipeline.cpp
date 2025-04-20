#include "Pipelines/BlurPipeline.h"

#include <Common/Math/Color.h>
#include <Common/Math/Primitives/Rectangle.h>
#include <Common/Math/Matrix4x4.h>
#include <Common/Memory/OffsetOf.h>
#include <Common/Threading/Jobs/JobBatch.h>

#include <Renderer/Vulkan/Includes.h>
#include <Renderer/Wrappers/RenderPassView.h>
#include <Renderer/Pipelines/PushConstantRange.h>
#include <Renderer/Assets/StaticMesh/RenderMeshView.h>
#include <Renderer/Commands/RenderCommandEncoderView.h>

#include <Renderer/Assets/Shader/ShaderCache.h>
#include <Renderer/Scene/ViewMatrices.h>
#include <Renderer/Devices/LogicalDevice.h>

namespace ngine::Rendering
{
	struct BlurConstants
	{
		Math::Vector2i m_blurDirection;
	};

	inline static constexpr Array<const PushConstantRange, 1> BlurPushConstantRanges = {
		PushConstantRange{ShaderStage::Compute, 0, sizeof(BlurConstants)}
	};

	inline static constexpr Array<const DescriptorSetLayout::Binding, 3> BlurDescriptorBindings = {
		DescriptorSetLayout::Binding::MakeSampledImage(
			0, ShaderStage::Compute, SampledImageType::UnfilterableFloat, ImageMappingType::TwoDimensional
		),
		DescriptorSetLayout::Binding::MakeStorageImage(1, ShaderStage::Compute, Format::R16G16B16A16_SFLOAT, StorageTextureAccess::WriteOnly),
		DescriptorSetLayout::Binding::MakeStorageBuffer(2, ShaderStage::Compute)
	};

	BlurPipeline::BlurPipeline(Rendering::LogicalDevice& logicalDevice, ShaderCache& shaderCache)
		: DescriptorSetLayout(logicalDevice, BlurDescriptorBindings)
	{
#if RENDERER_OBJECT_DEBUG_NAMES
		DescriptorSetLayout::SetDebugName(logicalDevice, "Blur");
#endif
		CreateBase(logicalDevice, ArrayView<const DescriptorSetLayoutView>{*this}, BlurPushConstantRanges);
		Create(logicalDevice, shaderCache, ShaderStageInfo{"B7E1F92A-E14E-49E6-998D-1CF0CA1887B4"_asset}, m_pipelineLayout);
	}

	void BlurPipeline::Destroy(LogicalDevice& logicalDevice)
	{
		ComputePipeline::Destroy(logicalDevice);
		DescriptorSetLayout::Destroy(logicalDevice);
	}

	void BlurPipeline::Compute(
		Rendering::LogicalDevice& logicalDevice,
		ArrayView<const DescriptorSetView, uint8> imageDescriptorSets,
		const ComputeCommandEncoderView computeCommandEncoder,
		const Math::Vector2ui resolution,
		const Math::Vector2i direction
	) const
	{
		BlurConstants constants;
		constants.m_blurDirection = direction;

		PushConstants(logicalDevice, computeCommandEncoder, BlurPushConstantRanges, constants);

		computeCommandEncoder.BindDescriptorSets(m_pipelineLayout, imageDescriptorSets, GetFirstDescriptorSetIndex());

		const Math::Vector3ui threadGroupSize{8, 8, 1};
		computeCommandEncoder.Dispatch(GetNumberOfThreadGroups(resolution, threadGroupSize), Math::Vector3ui{8, 8, 1});
	}
}
