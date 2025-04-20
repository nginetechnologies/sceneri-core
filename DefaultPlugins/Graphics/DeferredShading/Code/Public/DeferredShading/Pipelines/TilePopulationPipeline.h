#pragma once

#include <Common/Math/ForwardDeclarations/Vector2.h>
#include <Common/Math/ForwardDeclarations/Vector4.h>
#include <Common/Math/Primitives/ForwardDeclarations/Rectangle.h>
#include <Common/Math/ForwardDeclarations/Matrix4x4.h>
#include <Common/Math/ForwardDeclarations/Matrix3x3.h>

#include <Renderer/Pipelines/ComputePipeline.h>
#include <Renderer/Descriptors/DescriptorSetLayout.h>

namespace ngine::Rendering
{
	struct LogicalDeviceView;
	struct ComputeCommandEncoderView;
	struct ShaderCache;
	struct RenderPassView;
	struct RenderMeshView;
	struct TransformBufferView;
	struct ViewMatrices;

	struct InstanceGroupPerLogicalDeviceData;

	struct TilePopulationPipeline final : public DescriptorSetLayout, public ComputePipeline
	{
		inline static constexpr uint32 TileSize = 16;

		TilePopulationPipeline(
			Rendering::LogicalDevice& logicalDevice, ShaderCache& shaderCache, const DescriptorSetLayoutView viewInfoDescriptorSetLayout
		);
		TilePopulationPipeline(const TilePopulationPipeline&) = delete;
		TilePopulationPipeline& operator=(const TilePopulationPipeline&) = delete;
		TilePopulationPipeline(TilePopulationPipeline&& other) = default;
		TilePopulationPipeline& operator=(TilePopulationPipeline&&) = delete;

		[[nodiscard]] bool IsValid() const
		{
			return ComputePipeline::IsValid() & DescriptorSetLayout::IsValid();
		}

		void Destroy(LogicalDevice& logicalDevice);
		void Compute(
			Rendering::LogicalDevice& logicalDevice,
			ArrayView<const DescriptorSetView, uint8> descriptorSets,
			const ComputeCommandEncoderView computeCommandEncoder,
			const Math::Vector4ui lightCounts,
			const Math::Vector2ui tileRenderResolution,
			const Math::Vector2ui screenOffset
		) const;
	};
}
