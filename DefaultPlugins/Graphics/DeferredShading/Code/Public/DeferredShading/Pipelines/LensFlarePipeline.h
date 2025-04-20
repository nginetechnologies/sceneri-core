#pragma once

#include <Common/Math/ForwardDeclarations/Vector2.h>

#include <Renderer/Pipelines/ComputePipeline.h>
#include <Renderer/Descriptors/DescriptorSetLayout.h>

namespace ngine::Rendering
{
	struct LogicalDeviceView;
	struct RenderCommandEncoderView;
	struct ShaderCache;

	struct LensFlarePipeline final : public DescriptorSetLayout, public ComputePipeline
	{
		LensFlarePipeline(Rendering::LogicalDevice& logicalDevice, ShaderCache& shaderCache);
		LensFlarePipeline(const LensFlarePipeline&) = delete;
		LensFlarePipeline& operator=(const LensFlarePipeline&) = delete;
		LensFlarePipeline(LensFlarePipeline&& other) = default;
		LensFlarePipeline& operator=(LensFlarePipeline&&) = delete;

		[[nodiscard]] bool IsValid() const
		{
			return ComputePipeline::IsValid() & DescriptorSetLayout::IsValid();
		}

		void Destroy(LogicalDevice& logicalDevice);
		void Compute(
			Rendering::LogicalDevice& logicalDevice,
			ArrayView<const DescriptorSetView, uint8> imageDescriptorSets,
			const ComputeCommandEncoderView computeCommandEncoder,
			const Math::Vector2ui resolution
		) const;
	};
}
