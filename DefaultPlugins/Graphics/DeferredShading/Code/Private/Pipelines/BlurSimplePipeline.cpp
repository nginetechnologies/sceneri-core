#include "Pipelines/BlurSimplePipeline.h"

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
	struct BlurSimpleConstants
	{
		Math::Vector2i m_blurDirection;
	};

	inline static constexpr Array<const PushConstantRange, 1> BlurSimplePushConstantRanges = {
		PushConstantRange{ShaderStage::Compute, 0, sizeof(BlurSimpleConstants)}
	};

	inline static constexpr Array<const DescriptorSetLayout::Binding, 3> BlurSimpleDescriptorBindings = {
		DescriptorSetLayout::Binding::MakeSampledImage(
			0, ShaderStage::Compute, SampledImageType::UnfilterableFloat, ImageMappingType::TwoDimensional
		),
		DescriptorSetLayout::Binding::MakeStorageImage(1, ShaderStage::Compute, Format::R8_UNORM, StorageTextureAccess::WriteOnly),
		DescriptorSetLayout::Binding::MakeSampler(2, ShaderStage::Compute, SamplerBindingType::Filtering), // Linear, repeat,
	};

	BlurSimplePipeline::BlurSimplePipeline(Rendering::LogicalDevice& logicalDevice, ShaderCache& shaderCache)
		: DescriptorSetLayout(logicalDevice, BlurSimpleDescriptorBindings)
	{
#if RENDERER_OBJECT_DEBUG_NAMES
		DescriptorSetLayout::SetDebugName(logicalDevice, "Blur Simple");
#endif
		CreateBase(logicalDevice, ArrayView<const DescriptorSetLayoutView>{*this}, BlurSimplePushConstantRanges);

		Create(logicalDevice, shaderCache, ShaderStageInfo{"03055d2c-c38d-4de9-965d-7baad7bfd7b4"_asset}, m_pipelineLayout);
	}

	void BlurSimplePipeline::Destroy(LogicalDevice& logicalDevice)
	{
		ComputePipeline::Destroy(logicalDevice);
		DescriptorSetLayout::Destroy(logicalDevice);
	}

	void BlurSimplePipeline::Compute(
		Rendering::LogicalDevice& logicalDevice,
		ArrayView<const DescriptorSetView, uint8> imageDescriptorSets,
		const ComputeCommandEncoderView computeCommandEncoder,
		const Math::Vector2ui resolution,
		const Math::Vector2i direction
	) const
	{
		BlurSimpleConstants constants;
		constants.m_blurDirection = direction;

		PushConstants(logicalDevice, computeCommandEncoder, BlurSimplePushConstantRanges, constants);

		computeCommandEncoder.BindDescriptorSets(m_pipelineLayout, imageDescriptorSets, GetFirstDescriptorSetIndex());

		const Math::Vector3ui threadGroupSize{8, 8, 1};
		computeCommandEncoder.Dispatch(GetNumberOfThreadGroups(resolution, threadGroupSize), Math::Vector3ui{8, 8, 1});
	}
}
