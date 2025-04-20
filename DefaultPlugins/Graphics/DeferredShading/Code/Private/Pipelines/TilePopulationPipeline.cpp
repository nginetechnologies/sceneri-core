#include "Pipelines/TilePopulationPipeline.h"

#include <Common/Math/Color.h>
#include <Common/Math/Primitives/Rectangle.h>
#include <Common/Math/Matrix4x4.h>
#include <Common/Math/Vector2/Mod.h>
#include <Common/Math/Vector2/Sign.h>
#include <Common/Memory/OffsetOf.h>

#include <Renderer/Vulkan/Includes.h>
#include <Renderer/Wrappers/RenderPassView.h>
#include <Renderer/Assets/StaticMesh/RenderMeshView.h>
#include <Renderer/Devices/LogicalDevice.h>

#include <Renderer/Assets/Shader/ShaderCache.h>
#include <Renderer/Scene/ViewMatrices.h>
#include <Renderer/Devices/LogicalDeviceView.h>
#include <Renderer/Pipelines/PushConstantRange.h>

namespace ngine::Rendering
{
	struct TileConstants
	{
		Math::Vector4ui m_lightCounts;
		Math::Vector2ui m_screenResolution;
		Math::Vector2ui m_screenOffset;
	};

	inline static constexpr Array<const PushConstantRange, 1> TilePushConstantRanges = {
		PushConstantRange{ShaderStage::Compute, 0, sizeof(TileConstants)}
	};

	inline static constexpr Array<const DescriptorSetLayout::Binding, 4> TileDescriptorBindings = {
		DescriptorSetLayout::Binding::MakeStorageImage(0, ShaderStage::Compute, Format::R32G32B32A32_UINT, StorageTextureAccess::WriteOnly),
		DescriptorSetLayout::Binding::MakeStorageBuffer(1, ShaderStage::Compute),
		DescriptorSetLayout::Binding::MakeStorageBuffer(2, ShaderStage::Compute),
		DescriptorSetLayout::Binding::MakeStorageBuffer(3, ShaderStage::Compute)
	};

	TilePopulationPipeline::TilePopulationPipeline(
		Rendering::LogicalDevice& logicalDevice, ShaderCache& shaderCache, const DescriptorSetLayoutView viewInfoDescriptorSetLayout
	)
		: DescriptorSetLayout(logicalDevice, TileDescriptorBindings)
	{
#if RENDERER_OBJECT_DEBUG_NAMES
		DescriptorSetLayout::SetDebugName(logicalDevice, "Tile population");
#endif
		CreateBase(logicalDevice, Array<const DescriptorSetLayoutView, 2>{viewInfoDescriptorSetLayout, *this}, TilePushConstantRanges);
		Create(logicalDevice, shaderCache, ShaderStageInfo{"8a091355-5ca4-ed04-0642-ce3ed76a0813"_asset}, m_pipelineLayout);
	}

	void TilePopulationPipeline::Destroy(LogicalDevice& logicalDevice)
	{
		ComputePipeline::Destroy(logicalDevice);
		DescriptorSetLayout::Destroy(logicalDevice);
	}

	void TilePopulationPipeline::Compute(
		Rendering::LogicalDevice& logicalDevice,
		ArrayView<const DescriptorSetView, uint8> descriptorSets,
		const ComputeCommandEncoderView computeCommandEncoder,
		const Math::Vector4ui lightCounts,
		const Math::Vector2ui tileRenderResolution,
		const Math::Vector2ui screenOffset
	) const
	{
		const TileConstants constants = {lightCounts, tileRenderResolution, screenOffset};

		PushConstants(logicalDevice, computeCommandEncoder, TilePushConstantRanges, constants);

		computeCommandEncoder.BindDescriptorSets(m_pipelineLayout, descriptorSets, GetFirstDescriptorSetIndex());

		const Math::Vector3ui threadGroupSize{8, 8, 1};
		computeCommandEncoder.Dispatch(GetNumberOfThreadGroups(tileRenderResolution, threadGroupSize), Math::Vector3ui{8, 8, 1});
	}
}
