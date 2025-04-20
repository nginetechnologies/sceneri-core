#pragma once

#include <Common/Math/LinearGradient.h>

#include <Common/Math/ForwardDeclarations/Vector2.h>
#include <Common/Math/ForwardDeclarations/Angle.h>

#include <Renderer/Pipelines/GraphicsPipeline.h>

namespace ngine::Math
{
	template<typename T>
	struct TColor;
	using Color = TColor<float>;
}

namespace ngine::Rendering
{
	struct LogicalDeviceView;
	struct RenderCommandEncoderView;
	struct ShaderCache;
	struct RenderPassView;

	struct UniformRoundedRectangleLinearGradientPipeline final : public GraphicsPipeline
	{
		UniformRoundedRectangleLinearGradientPipeline(Rendering::LogicalDevice& logicalDevice);
		UniformRoundedRectangleLinearGradientPipeline(const UniformRoundedRectangleLinearGradientPipeline&) = delete;
		UniformRoundedRectangleLinearGradientPipeline& operator=(const UniformRoundedRectangleLinearGradientPipeline&) = delete;
		UniformRoundedRectangleLinearGradientPipeline(UniformRoundedRectangleLinearGradientPipeline&& other) = default;
		UniformRoundedRectangleLinearGradientPipeline& operator=(UniformRoundedRectangleLinearGradientPipeline&&) = delete;

		void Draw(
			LogicalDevice& logicalDevice,
			const RenderCommandEncoderView renderCommandEncoder,
			const Math::Vector2f positionRatio,
			Math::Vector2f sizeRatio,
			const Math::Anglef angle,
			const float depth,
			const Math::Color color,
			const float roundingRadius,
			const float aspectRatio,
			const Math::LinearGradient& gradient
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
