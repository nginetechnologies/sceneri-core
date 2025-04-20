#pragma once

#include <Common/Math/ForwardDeclarations/Vector2.h>

#include <Renderer/Pipelines/ComputePipeline.h>
#include <Renderer/Descriptors/DescriptorSetLayout.h>

namespace ngine::Rendering
{
	struct LogicalDeviceView;
	struct RenderCommandEncoderView;
	struct ShaderCache;

	struct SSRPipeline final : public DescriptorSetLayout, public ComputePipeline
	{
		SSRPipeline(
			Rendering::LogicalDevice& logicalDevice, ShaderCache& shaderCache, const DescriptorSetLayoutView viewInfoDescriptorSetLayout
		);
		SSRPipeline(const SSRPipeline&) = delete;
		SSRPipeline& operator=(const SSRPipeline&) = delete;
		SSRPipeline(SSRPipeline&& other) = default;
		SSRPipeline& operator=(SSRPipeline&&) = delete;

		[[nodiscard]] bool IsValid() const
		{
			return ComputePipeline::IsValid() & DescriptorSetLayout::IsValid();
		}

		void Destroy(LogicalDevice& logicalDevice);
		void Compute(
			Rendering::LogicalDevice& logicalDevice,
			float nearPlane,
			ArrayView<const DescriptorSetView, uint8> imageDescriptorSets,
			const ComputeCommandEncoderView computeCommandEncoder,
			const Math::Vector2ui resolution
		) const;
	};
}
