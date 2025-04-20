#pragma once

#include <Common/Math/ForwardDeclarations/Vector2.h>
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

	struct DrawTextPipeline final : public Rendering::DescriptorSetLayout, public Rendering::GraphicsPipeline
	{
		DrawTextPipeline(Rendering::LogicalDevice& logicalDevice);
		DrawTextPipeline(const DrawTextPipeline&) = delete;
		DrawTextPipeline& operator=(const DrawTextPipeline&) = delete;
		DrawTextPipeline(DrawTextPipeline&& other) = default;
		DrawTextPipeline& operator=(DrawTextPipeline&&) = delete;

		[[nodiscard]] bool IsValid() const
		{
			return GraphicsPipeline::IsValid() & DescriptorSetLayout::IsValid();
		}

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
