#pragma once

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

	struct RectangleGridPipeline final : public GraphicsPipeline
	{
		RectangleGridPipeline(Rendering::LogicalDevice& logicalDevice);
		RectangleGridPipeline(const RectangleGridPipeline&) = delete;
		RectangleGridPipeline& operator=(const RectangleGridPipeline&) = delete;
		RectangleGridPipeline(RectangleGridPipeline&& other) = default;
		RectangleGridPipeline& operator=(RectangleGridPipeline&&) = delete;

		void Draw(
			LogicalDevice& logicalDevice,
			const RenderCommandEncoderView renderCommandEncoder,
			const Math::Vector2f positionRatio,
			Math::Vector2f sizeRatio,
			const Math::Anglef angle,
			const float depth,
			const Math::Color backgroundColor,
			const Math::Color gridColor,
			const Math::Vector2f offset,
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
