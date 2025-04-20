#pragma once

#include <Widgets/Pipelines/RectanglePipeline.h>
#include <Widgets/Pipelines/BorderRectanglePipeline.h>
#include <Widgets/Pipelines/RectangleLinearGradientPipeline.h>
#include <Widgets/Pipelines/RectangleGridPipeline.h>
#include <Widgets/Pipelines/LinePipeline.h>

#include <Widgets/Pipelines/UniformRoundedRectanglePipeline.h>
#include <Widgets/Pipelines/UniformRoundedBorderRectanglePipeline.h>
#include <Widgets/Pipelines/UniformRoundedRectangleLinearGradientPipeline.h>

#include <Widgets/Pipelines/RoundedRectanglePipeline.h>
#include <Widgets/Pipelines/RoundedBorderRectanglePipeline.h>
#include <Widgets/Pipelines/RoundedRectangleLinearGradientPipeline.h>

#include <Widgets/Pipelines/CirclePipeline.h>
#include <Widgets/Pipelines/CircleLinearGradientPipeline.h>
#include <Widgets/Pipelines/CircleConicGradientPipeline.h>

#include <FontRendering/FontPipeline.h>
#include <FontRendering/FontGradientPipeline.h>

#include <Widgets/Pipelines/ColoredIconPipeline.h>
#include <Widgets/Pipelines/RoundedImagePipeline.h>

namespace ngine::Rendering
{
	struct DescriptorSetView;

	struct Pipelines
	{
		Pipelines(LogicalDevice& logicalDevice, const ArrayView<const DescriptorSetLayoutView> imageDescriptorSetLayouts);
		Pipelines(const Pipelines&) = delete;
		Pipelines& operator=(const Pipelines&) = delete;
		Pipelines(Pipelines&&) = default;
		Pipelines& operator=(Pipelines&&) = delete;
		~Pipelines() = default;

		void Destroy(LogicalDevice& logicalDevice);

		[[nodiscard]] const RectanglePipeline& GetRectanglePipeline() const
		{
			return m_rectanglePipeline;
		}
		[[nodiscard]] const BorderRectanglePipeline& GetBorderRectanglePipeline() const
		{
			return m_borderRectanglePipeline;
		}
		[[nodiscard]] const RectangleLinearGradientPipeline& GetGradientRectanglePipeline() const
		{
			return m_gradientRectanglePipeline;
		}
		[[nodiscard]] const RectangleGridPipeline& GetRectangleGridPipeline() const
		{
			return m_gridRectanglePipeline;
		}
		[[nodiscard]] const LinePipeline& GetLinePipeline() const
		{
			return m_linePipeline;
		}

		[[nodiscard]] const UniformRoundedRectanglePipeline& GetUniformRoundedRectanglePipeline() const
		{
			return m_uniformRoundedRectanglePipeline;
		}
		[[nodiscard]] const UniformRoundedBorderRectanglePipeline& GetUniformRoundedBorderRectanglePipeline() const
		{
			return m_uniformRoundedBorderRectanglePipeline;
		}
		[[nodiscard]] const UniformRoundedRectangleLinearGradientPipeline& GetUniformRoundedGradientRectanglePipeline() const
		{
			return m_uniformRoundedGradientRectanglePipeline;
		}

		[[nodiscard]] const RoundedRectanglePipeline& GetRoundedRectanglePipeline() const
		{
			return m_roundedRectanglePipeline;
		}
		[[nodiscard]] const RoundedBorderRectanglePipeline& GetRoundedBorderRectanglePipeline() const
		{
			return m_roundedBorderRectanglePipeline;
		}
		[[nodiscard]] const RoundedRectangleLinearGradientPipeline& GetRoundedGradientRectanglePipeline() const
		{
			return m_roundedGradientRectanglePipeline;
		}

		[[nodiscard]] const CirclePipeline& GetCirclePipeline() const
		{
			return m_circlePipeline;
		}
		[[nodiscard]] const CircleLinearGradientPipeline& GetCircleLinearGradientPipeline() const
		{
			return m_circleLinearGradientPipeline;
		}
		[[nodiscard]] const CircleConicGradientPipeline& GetCircleConicGradientPipeline() const
		{
			return m_circleConicGradientPipeline;
		}

		[[nodiscard]] const Font::FontPipeline& GetFontPipeline() const
		{
			return m_fontPipeline;
		}
		[[nodiscard]] const Font::FontGradientPipeline& GetFontGradientPipeline() const
		{
			return m_fontGradientPipeline;
		}

		[[nodiscard]] const ColoredIconPipeline& GetColoredIconPipeline() const
		{
			return m_coloredIconPipeline;
		}
		[[nodiscard]] const RoundedImagePipeline& GetRoundedImagePipeline() const
		{
			return m_roundedImagePipeline;
		}

		[[nodiscard]] bool IsValid() const
		{
			return m_rectanglePipeline.IsValid() & m_borderRectanglePipeline.IsValid() & m_gradientRectanglePipeline.IsValid() &

			       m_uniformRoundedRectanglePipeline.IsValid() & m_uniformRoundedBorderRectanglePipeline.IsValid() &
			       m_uniformRoundedGradientRectanglePipeline.IsValid() &

			       m_roundedRectanglePipeline.IsValid() & m_roundedBorderRectanglePipeline.IsValid() &
			       m_roundedGradientRectanglePipeline.IsValid() &

			       m_circlePipeline.IsValid() & m_circleLinearGradientPipeline.IsValid() & m_circleConicGradientPipeline.IsValid() &

			       m_fontPipeline.IsValid() & m_fontGradientPipeline.IsValid() &

			       m_coloredIconPipeline.IsValid() & m_roundedImagePipeline.IsValid();
		}

		void PrepareForResize(const LogicalDeviceView logicalDevice);
		[[nodiscard]] Threading::JobBatch Create(
			LogicalDevice& logicalDevice,
			struct ShaderCache& shaderCache,
			const struct RenderPassView renderPass,
			const Math::Rectangleui outputArea,
			const Math::Rectangleui renderArea,
			const uint8 subpassIndex
		);
	protected:
		RectanglePipeline m_rectanglePipeline;
		BorderRectanglePipeline m_borderRectanglePipeline;
		RectangleLinearGradientPipeline m_gradientRectanglePipeline;
		RectangleGridPipeline m_gridRectanglePipeline;
		LinePipeline m_linePipeline;

		UniformRoundedRectanglePipeline m_uniformRoundedRectanglePipeline;
		UniformRoundedBorderRectanglePipeline m_uniformRoundedBorderRectanglePipeline;
		UniformRoundedRectangleLinearGradientPipeline m_uniformRoundedGradientRectanglePipeline;

		RoundedRectanglePipeline m_roundedRectanglePipeline;
		RoundedBorderRectanglePipeline m_roundedBorderRectanglePipeline;
		RoundedRectangleLinearGradientPipeline m_roundedGradientRectanglePipeline;

		CirclePipeline m_circlePipeline;
		CircleLinearGradientPipeline m_circleLinearGradientPipeline;
		CircleConicGradientPipeline m_circleConicGradientPipeline;

		Font::FontPipeline m_fontPipeline;
		Font::FontGradientPipeline m_fontGradientPipeline;

		ColoredIconPipeline m_coloredIconPipeline;
		RoundedImagePipeline m_roundedImagePipeline;
	};
}
