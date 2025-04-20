#pragma once

#include <Common/Math/ForwardDeclarations/Vector2.h>
#include <Common/Math/ForwardDeclarations/Color.h>
#include <Common/Math/ForwardDeclarations/Angle.h>

#include <Renderer/Pipelines/GraphicsPipeline.h>

namespace ngine::Rendering
{
	struct LogicalDeviceView;
	struct RenderCommandEncoderView;
	struct ShaderCache;
	struct RenderPassView;
	struct DescriptorSetView;

	struct RoundedImagePipeline final : public GraphicsPipeline
	{
		RoundedImagePipeline(
			Rendering::LogicalDevice& logicalDevice, const ArrayView<const Rendering::DescriptorSetLayoutView> descriptorSetLayouts
		);
		RoundedImagePipeline(const RoundedImagePipeline&) = delete;
		RoundedImagePipeline& operator=(const RoundedImagePipeline&) = delete;
		RoundedImagePipeline(RoundedImagePipeline&& other) = default;
		RoundedImagePipeline& operator=(RoundedImagePipeline&&) = delete;
		~RoundedImagePipeline() = default;

		void Draw(
			LogicalDevice& logicalDevice,
			const RenderCommandEncoderView renderCommandEncoder,
			const Math::Vector2f positionRatio,
			const Math::Vector2f sizeRatio,
			const Math::Anglef angle,
			const Math::Color color,
			const float roundingRadius,
			const float aspectRatio,
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
