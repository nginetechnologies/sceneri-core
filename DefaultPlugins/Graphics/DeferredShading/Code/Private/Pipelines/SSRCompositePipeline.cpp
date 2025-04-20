#include "Pipelines/SSRCompositePipeline.h"

#include <Renderer/Vulkan/Includes.h>

#include <Renderer/Assets/Shader/ShaderCache.h>
#include <Renderer/Devices/LogicalDevice.h>

#include <Common/Math/Vector2.h>
#include <Common/Math/Vector3.h>

namespace ngine::Rendering
{
	inline static constexpr Array<const DescriptorSetLayout::Binding, 2> SSRDescriptorBindings = {
		DescriptorSetLayout::Binding::MakeStorageImage(0, ShaderStage::Compute, Format::R16G16B16A16_SFLOAT, StorageTextureAccess::WriteOnly),
		DescriptorSetLayout::Binding::MakeSampledImage(
			1, ShaderStage::Compute, SampledImageType::UnfilterableFloat, ImageMappingType::TwoDimensional
		)
	};

	SSRCompositePipeline::SSRCompositePipeline(Rendering::LogicalDevice& logicalDevice, ShaderCache& shaderCache)
		: DescriptorSetLayout(logicalDevice, SSRDescriptorBindings)
	{
#if RENDERER_OBJECT_DEBUG_NAMES
		DescriptorSetLayout::SetDebugName(logicalDevice, "SSR Composite");
#endif
		CreateBase(logicalDevice, ArrayView<const DescriptorSetLayoutView>{*this}, {});
		Create(logicalDevice, shaderCache, ShaderStageInfo{"90F0187C-7E36-423C-9F34-7A802A626FC2"_asset}, m_pipelineLayout);
	}

	void SSRCompositePipeline::Destroy(LogicalDevice& logicalDevice)
	{
		ComputePipeline::Destroy(logicalDevice);
		DescriptorSetLayout::Destroy(logicalDevice);
	}

	void SSRCompositePipeline::Compute(
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
