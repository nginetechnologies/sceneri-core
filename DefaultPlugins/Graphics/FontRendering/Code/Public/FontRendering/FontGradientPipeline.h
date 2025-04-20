#pragma once

#include <Common/Math/ForwardDeclarations/Vector2.h>
#include <Common/Math/LinearGradient.h>
#include <Common/Memory/Containers/StringView.h>

#include <Renderer/Pipelines/GraphicsPipeline.h>
#include <Renderer/Descriptors/DescriptorSetLayout.h>

namespace ngine::Math
{
	template<typename T>
	struct TColor;
	using Color = TColor<float>;
}

namespace Rendering
{
	struct LogicalDeviceView;
	struct RenderCommandEncoderView;
	struct ShaderCache;
	struct RenderPassView;
}

namespace ngine::Font
{
	struct Atlas;

	struct FontGradientPipeline final : public Rendering::DescriptorSetLayout, public Rendering::GraphicsPipeline
	{
		FontGradientPipeline(Rendering::LogicalDevice& logicalDevice);
		FontGradientPipeline(const FontGradientPipeline&) = delete;
		FontGradientPipeline& operator=(const FontGradientPipeline&) = delete;
		FontGradientPipeline(FontGradientPipeline&& other) = default;
		FontGradientPipeline& operator=(FontGradientPipeline&&) = delete;

		void Destroy(Rendering::LogicalDevice& logicalDevice);
		void Draw(
			Rendering::LogicalDevice& logicalDevice,
			Rendering::RenderCommandEncoderView renderCommandEncoder,
			const Atlas& fontAtlas,
			const Rendering::DescriptorSetView descriptorSet,
			const ConstUnicodeStringView text,
			const Math::Vector2f positionRatio,
			const Math::Vector2f renderSizeRatio,
			const Math::Color color,
			const float depth,
			const Math::LinearGradient& gradient
		) const;

		[[nodiscard]] bool IsValid() const
		{
			return DescriptorSetLayout::IsValid() & GraphicsPipeline::IsValid();
		}
		[[nodiscard]] Threading::JobBatch CreatePipeline(
			Rendering::LogicalDevice& logicalDevice,
			Rendering::ShaderCache& shaderCache,
			const Rendering::RenderPassView renderPass,
			const Math::Rectangleui outputArea,
			const Math::Rectangleui renderArea,
			const uint8 subpassIndex
		);
	};
}
