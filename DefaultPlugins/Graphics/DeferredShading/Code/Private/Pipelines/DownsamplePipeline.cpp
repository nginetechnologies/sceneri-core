#include "Pipelines/DownsamplePipeline.h"

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
#include <Renderer/Commands/ComputeCommandEncoderView.h>

namespace ngine::Rendering
{
	inline static constexpr Array<const DescriptorSetLayout::Binding, 2> DownsampleDescriptorBindings = {
		DescriptorSetLayout::Binding::MakeSampledImage(
			0, ShaderStage::Compute, SampledImageType::UnfilterableFloat, ImageMappingType::TwoDimensional
		),
		DescriptorSetLayout::Binding::MakeStorageImage(1, ShaderStage::Compute, Format::R16G16B16A16_SFLOAT, StorageTextureAccess::WriteOnly)
	};

	DownsamplePipeline::DownsamplePipeline(Rendering::LogicalDevice& logicalDevice, ShaderCache& shaderCache)
		: DescriptorSetLayout(logicalDevice, DownsampleDescriptorBindings)
	{
#if RENDERER_OBJECT_DEBUG_NAMES
		DescriptorSetLayout::SetDebugName(logicalDevice, "Downsample");
#endif
		CreateBase(logicalDevice, Array<const DescriptorSetLayoutView, 1>{*this}, {});
		Create(logicalDevice, shaderCache, ShaderStageInfo{"CA020D95-6935-4756-9F8C-5D988D5585EC"_asset}, m_pipelineLayout);
	}

	void DownsamplePipeline::Destroy(LogicalDevice& logicalDevice)
	{
		ComputePipeline::Destroy(logicalDevice);
		DescriptorSetLayout::Destroy(logicalDevice);
	}

	void DownsamplePipeline::Compute(
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
