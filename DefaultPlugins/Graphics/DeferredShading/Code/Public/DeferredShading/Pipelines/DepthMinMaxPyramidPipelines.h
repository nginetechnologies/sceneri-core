#pragma once

#include <Common/Math/ForwardDeclarations/Vector2.h>

#include <Renderer/Pipelines/ComputePipeline.h>
#include <Renderer/Descriptors/DescriptorSetLayout.h>

namespace ngine::Rendering
{
	struct LogicalDeviceView;
	struct RenderCommandEncoderView;
	struct ShaderCache;

	struct InitialDepthMinMaxPyramidPipeline final : public DescriptorSetLayout, public ComputePipeline
	{
		InitialDepthMinMaxPyramidPipeline(Rendering::LogicalDevice& logicalDevice, ShaderCache& shaderCache);
		InitialDepthMinMaxPyramidPipeline(const InitialDepthMinMaxPyramidPipeline&) = delete;
		InitialDepthMinMaxPyramidPipeline& operator=(const InitialDepthMinMaxPyramidPipeline&) = delete;
		InitialDepthMinMaxPyramidPipeline(InitialDepthMinMaxPyramidPipeline&& other) = default;
		InitialDepthMinMaxPyramidPipeline& operator=(InitialDepthMinMaxPyramidPipeline&&) = delete;

		[[nodiscard]] bool IsValid() const
		{
			return ComputePipeline::IsValid() & DescriptorSetLayout::IsValid();
		}

		void Destroy(LogicalDevice& logicalDevice);
		void Compute(
			ArrayView<const DescriptorSetView, uint8> imageDescriptorSets,
			const ComputeCommandEncoderView computeCommandEncoder,
			const Math::Vector2ui resolution
		) const;
	};

	struct DepthMinMaxPyramidPipeline final : public DescriptorSetLayout, public ComputePipeline
	{
		DepthMinMaxPyramidPipeline(Rendering::LogicalDevice& logicalDevice, ShaderCache& shaderCache);
		DepthMinMaxPyramidPipeline(const DepthMinMaxPyramidPipeline&) = delete;
		DepthMinMaxPyramidPipeline& operator=(const DepthMinMaxPyramidPipeline&) = delete;
		DepthMinMaxPyramidPipeline(DepthMinMaxPyramidPipeline&& other) = default;
		DepthMinMaxPyramidPipeline& operator=(DepthMinMaxPyramidPipeline&&) = delete;

		[[nodiscard]] bool IsValid() const
		{
			return ComputePipeline::IsValid() & DescriptorSetLayout::IsValid();
		}

		void Destroy(LogicalDevice& logicalDevice);
		void Compute(
			ArrayView<const DescriptorSetView, uint8> imageDescriptorSets,
			const ComputeCommandEncoderView computeCommandEncoder,
			const Math::Vector2ui resolution
		) const;
	};
}
