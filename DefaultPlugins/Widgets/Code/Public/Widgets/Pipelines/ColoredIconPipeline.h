#pragma once

#include <Common/Math/ForwardDeclarations/Vector2.h>
#include <Common/Math/ForwardDeclarations/Color.h>
#include <Common/Math/ForwardDeclarations/Angle.h>

#include <Renderer/Pipelines/GraphicsPipeline.h>

namespace ngine::Rendering
{
	struct LogicalDevice;
	struct RenderCommandEncoderView;
	struct ShaderCache;
	struct RenderPassView;
	struct DescriptorSetView;

	struct ColoredIconPipeline final : public GraphicsPipeline
	{
		ColoredIconPipeline(Rendering::LogicalDevice& logicalDevice, const ArrayView<const DescriptorSetLayoutView> descriptorSetLayouts);
		ColoredIconPipeline(const ColoredIconPipeline&) = delete;
		ColoredIconPipeline& operator=(const ColoredIconPipeline&) = delete;
		ColoredIconPipeline(ColoredIconPipeline&& other) = default;
		ColoredIconPipeline& operator=(ColoredIconPipeline&&) = delete;
		~ColoredIconPipeline() = default;

		void Draw(
			LogicalDevice& logicalDevice,
			const RenderCommandEncoderView renderCommandEncoder,
			const Math::Vector2f positionRatio,
			const Math::Vector2f sizeRatio,
			const Math::Anglef angle,
			const Math::Color color,
			const float depth,
			const DescriptorSetView imageDescriptorSet
		) const;

		[[nodiscard]] Threading::JobBatch CreatePipeline(
			LogicalDevice& logicalDevice,
			ShaderCache& shaderCache,
			const RenderPassView renderPass,
			const Math::Rectangleui outputArea,
			const Math::Rectangleui renderArea,
			const uint8 subpassIndex
		);
	};
}
