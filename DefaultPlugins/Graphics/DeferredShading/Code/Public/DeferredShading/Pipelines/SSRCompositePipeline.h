#pragma once

#include <Common/Math/ForwardDeclarations/Vector2.h>

#include <Renderer/Pipelines/ComputePipeline.h>
#include <Renderer/Descriptors/DescriptorSetLayout.h>

namespace ngine::Rendering
{
	struct LogicalDeviceView;
	struct RenderCommandEncoderView;
	struct ShaderCache;

	struct SSRCompositePipeline final : public DescriptorSetLayout, public ComputePipeline
	{
		SSRCompositePipeline(Rendering::LogicalDevice& logicalDevice, ShaderCache& shaderCache);
		SSRCompositePipeline(const SSRCompositePipeline&) = delete;
		SSRCompositePipeline& operator=(const SSRCompositePipeline&) = delete;
		SSRCompositePipeline(SSRCompositePipeline&& other) = default;
		SSRCompositePipeline& operator=(SSRCompositePipeline&&) = delete;

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
