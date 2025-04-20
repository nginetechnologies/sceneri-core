#pragma once

#include <Common/Math/ForwardDeclarations/Vector2.h>
#include <Common/Memory/Containers/StringView.h>

#include <Renderer/Pipelines/GraphicsPipeline.h>

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

	struct FontPipeline final : public Rendering::GraphicsPipeline
	{
		FontPipeline(Rendering::LogicalDevice& logicalDevice, const ArrayView<const Rendering::DescriptorSetLayoutView> descriptorSetLayouts);
		FontPipeline(const FontPipeline&) = delete;
		FontPipeline& operator=(const FontPipeline&) = delete;
		FontPipeline(FontPipeline&& other) = default;
		FontPipeline& operator=(FontPipeline&&) = delete;

		void Draw(
			Rendering::LogicalDevice& logicalDevice,
			Rendering::RenderCommandEncoderView renderCommandEncoder,
			const Atlas& fontAtlas,
			const Rendering::DescriptorSetView descriptorSet,
			const ConstUnicodeStringView text,
			const Math::Vector2f positionRatio,
			const Math::Vector2f renderSizeRatio,
			const Math::Color color,
			const float depth
		) const;

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
