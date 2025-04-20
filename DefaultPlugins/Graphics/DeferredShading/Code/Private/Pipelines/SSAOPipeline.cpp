#include "Pipelines/SSAOPipeline.h"

#include <Renderer/Vulkan/Includes.h>
#include <Renderer/Wrappers/RenderPassView.h>
#include <Renderer/Assets/StaticMesh/RenderMeshView.h>

#include <Renderer/Assets/Shader/ShaderCache.h>
#include <Renderer/Scene/ViewMatrices.h>
#include <Renderer/Devices/LogicalDevice.h>
#include <Renderer/Pipelines/PushConstantRange.h>

#include <Common/Math/Color.h>
#include <Common/Math/Primitives/Rectangle.h>
#include <Common/Math/Matrix4x4.h>
#include <Common/Memory/OffsetOf.h>
#include <Common/Memory/Containers/FlatVector.h>

namespace ngine::Rendering
{
	inline static constexpr Array SSAODescriptorBindings = {
		DescriptorSetLayout::Binding::MakeSampledImage(0, ShaderStage::Compute, SampledImageType::Depth, ImageMappingType::TwoDimensional),
		DescriptorSetLayout::Binding::MakeSampledImage(
			1, ShaderStage::Compute, SampledImageType::UnfilterableFloat, ImageMappingType::TwoDimensional
		),
		DescriptorSetLayout::Binding::MakeSampler(2, ShaderStage::Compute, SamplerBindingType::Filtering),
		DescriptorSetLayout::Binding::MakeSampledImage(
			3, ShaderStage::Compute, SampledImageType::UnfilterableFloat, ImageMappingType::TwoDimensional
		),
		// DescriptorSetLayout::Binding::MakeStorageImage(3, ShaderStage::Compute, StorageTextureAccess::WriteOnly), // SSAO buffer
		DescriptorSetLayout::Binding::MakeStorageImage(
			4, ShaderStage::Compute, Format::R16G16B16A16_SFLOAT, StorageTextureAccess::WriteOnly
		), // HDRScene
		DescriptorSetLayout::Binding::MakeStorageBuffer(5, ShaderStage::Compute)
	};

	SSAOPipeline::SSAOPipeline(
		Rendering::LogicalDevice& logicalDevice, ShaderCache& shaderCache, const DescriptorSetLayoutView viewInfoDescriptorSetLayout
	)
		: DescriptorSetLayout(logicalDevice, SSAODescriptorBindings)
	{
#if RENDERER_OBJECT_DEBUG_NAMES
		DescriptorSetLayout::SetDebugName(logicalDevice, "SSAO");
#endif
		CreateBase(logicalDevice, Array<const DescriptorSetLayoutView, 2>{viewInfoDescriptorSetLayout, *this});
		Create(logicalDevice, shaderCache, ShaderStageInfo{"bd8bc9d5-968b-4b06-bc28-0962ab61bb0d"_asset}, m_pipelineLayout);
	}

	void SSAOPipeline::Destroy(LogicalDevice& logicalDevice)
	{
		ComputePipeline::Destroy(logicalDevice);
		DescriptorSetLayout::Destroy(logicalDevice);
	}

	void SSAOPipeline::Compute(
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
