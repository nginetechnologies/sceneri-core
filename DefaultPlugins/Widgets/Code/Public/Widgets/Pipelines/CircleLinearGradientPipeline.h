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

	struct CircleLinearGradientPipeline final : public GraphicsPipeline
	{
		CircleLinearGradientPipeline(Rendering::LogicalDevice& logicalDevice);
		CircleLinearGradientPipeline(const CircleLinearGradientPipeline&) = delete;
		CircleLinearGradientPipeline& operator=(const CircleLinearGradientPipeline&) = delete;
		CircleLinearGradientPipeline(CircleLinearGradientPipeline&& other) = default;
		CircleLinearGradientPipeline& operator=(CircleLinearGradientPipeline&&) = delete;

		void Draw(
			LogicalDevice& logicalDevice,
			const RenderCommandEncoderView renderCommandEncoder,
			const Math::Vector2f positionRatio,
			Math::Vector2f sizeRatio,
			const Math::Anglef angle,
			const float depth,
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
