#pragma once

#include <Common/Math/ForwardDeclarations/Vector2.h>
#include <Common/Math/ForwardDeclarations/Angle.h>
#include <Common/Math/Primitives/ForwardDeclarations/Spline.h>

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

	struct LinePipeline final : public GraphicsPipeline
	{
		LinePipeline(Rendering::LogicalDevice& logicalDevice);
		LinePipeline(const LinePipeline&) = delete;
		LinePipeline& operator=(const LinePipeline&) = delete;
		LinePipeline(LinePipeline&& other) = default;
		LinePipeline& operator=(LinePipeline&&) = delete;

		void Draw(
			LogicalDevice& logicalDevice,
			const RenderCommandEncoderView renderCommandEncoder,
			const Math::Vector2f positionRatio,
			Math::Vector2f sizeRatio,
			const Math::Anglef angle,
			const float depth,
			const Math::Spline2f& spline,
			const Math::Color lineColor,
			const float thickness,
			const float aspectRatio
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
