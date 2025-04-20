#pragma once

#include <Common/Math/ForwardDeclarations/Vector2.h>
#include <Common/Math/ForwardDeclarations/Vector4.h>
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

	struct BorderRectanglePipeline final : public GraphicsPipeline
	{
		BorderRectanglePipeline(Rendering::LogicalDevice& logicalDevice);
		BorderRectanglePipeline(const BorderRectanglePipeline&) = delete;
		BorderRectanglePipeline& operator=(const BorderRectanglePipeline&) = delete;
		BorderRectanglePipeline(BorderRectanglePipeline&& other) = default;
		BorderRectanglePipeline& operator=(BorderRectanglePipeline&&) = delete;

		void Draw(
			LogicalDevice& logicalDevice,
			const RenderCommandEncoderView renderCommandEncoder,
			const Math::Vector2f positionRatio,
			Math::Vector2f sizeRatio,
			const Math::Anglef angle,
			const float depth,
			const Math::Color color,
			const Math::Vector2f innerSize
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
