#include "FontGradientPipeline.h"

#include <Renderer/Vulkan/Includes.h>

#include <FontRendering/Font.h>
#include <FontRendering/FontAtlas.h>
#include <Renderer/Devices/LogicalDevice.h>
#include <Renderer/Pipelines/PushConstantRange.h>

#include <Common/Math/Color.h>
#include <Common/Math/LinearGradient.h>
#include <Common/Math/Vector3.h>
#include <Common/Math/Vector2/Max.h>
#include <Common/Math/Rotation2D.h>
#include <Common/Memory/OffsetOf.h>
#include <Common/Math/Vector2/Round.h>
#include <Common/Memory/Containers/FlatVector.h>
#include <Common/Threading/Jobs/JobBatch.h>

namespace ngine::Font
{
	struct FontGradientConstants
	{
		struct VertexConstants
		{
			Math::Vector2f positionRatio;
			Math::Vector2f sizeRatio;
			float depth;
		};

		struct alignas(16) FragmentConstants
		{
			Math::PackedColor32 color;
			Math::Vector2f uvOrigin;
			Math::Vector2f uvScale;
			Math::Vector2f uvGradientScale;
			Math::Vector2f uvGradientOffset;
			Math::PackedColor32 colors[3];
			Math::Vector2f points[3];
		};

		VertexConstants vertexConstants;
		FragmentConstants fragmentConstants;
	};

	inline static constexpr Array<const Rendering::DescriptorSetLayout::Binding, 2> FontGradientPipelineDescriptorBindings = {
		Rendering::DescriptorSetLayout::Binding::MakeSampledImage(
			0, Rendering::ShaderStage::Fragment, Rendering::SampledImageType::Float, Rendering::ImageMappingType::TwoDimensional
		),
		Rendering::DescriptorSetLayout::Binding::MakeSampler(1, Rendering::ShaderStage::Fragment, Rendering::SamplerBindingType::Filtering)
	};

	inline static constexpr Array<const Rendering::PushConstantRange, 2> FontGradientPipelinePushConstantRanges = {
		Rendering::PushConstantRange{
			Rendering::ShaderStage::Vertex,
			static_cast<uint32>(OFFSET_OF(FontGradientConstants, vertexConstants)),
			sizeof(FontGradientConstants::VertexConstants)
		},
		Rendering::PushConstantRange{
			Rendering::ShaderStage::Fragment,
			static_cast<uint32>(OFFSET_OF(FontGradientConstants, fragmentConstants)),
			sizeof(FontGradientConstants::FragmentConstants)
		}
	};

	FontGradientPipeline::FontGradientPipeline(Rendering::LogicalDevice& logicalDevice)
		: DescriptorSetLayout(logicalDevice, FontGradientPipelineDescriptorBindings)
	{
#if RENDERER_OBJECT_DEBUG_NAMES
		DescriptorSetLayout::SetDebugName(logicalDevice, "Font Gradient");
#endif
		CreateBase(logicalDevice, ArrayView<const DescriptorSetLayoutView>{*this}, FontGradientPipelinePushConstantRanges);
	}

	void FontGradientPipeline::Destroy(Rendering::LogicalDevice& logicalDevice)
	{
		GraphicsPipeline::Destroy(logicalDevice);
		DescriptorSetLayout::Destroy(logicalDevice);
	}

	Threading::JobBatch FontGradientPipeline::CreatePipeline(
		Rendering::LogicalDevice& logicalDevice,
		Rendering::ShaderCache& shaderCache,
		const Rendering::RenderPassView renderPass,
		const Math::Rectangleui outputArea,
		const Math::Rectangleui renderArea,
		const uint8 subpassIndex
	)
	{
		using namespace Rendering;

		const VertexStageInfo vertexStage{"7AE3BFE2-5D04-4D3C-B09E-7F282452A40A"_asset};

		const PrimitiveInfo primitiveInfo{PrimitiveTopology::TriangleList, PolygonMode::Fill, WindingOrder::CounterClockwise, CullMode::Front};

		const Array<Viewport, 1> viewports{Viewport{outputArea}};
		const Array<Math::Rectangleui, 1> scissors{renderArea};

		const ColorTargetInfo colorBlendAttachment{
			ColorAttachmentBlendState{BlendFactor::One, BlendFactor::OneMinusSourceAlpha, BlendOperation::Add},
			AlphaAttachmentBlendState{BlendFactor::One, BlendFactor::One, BlendOperation::Add}
		};
		const FragmentStageInfo fragmentStage{
			ShaderStageInfo{"71bebbba-fb9e-4c2a-b6f5-9a947e8aec97"_asset},
			ArrayView<const ColorTargetInfo>(colorBlendAttachment)
		};

		const DepthStencilInfo depthStencil{DepthStencilFlags::DepthTest | DepthStencilFlags::DepthWrite, CompareOperation::GreaterOrEqual};

		const EnumFlags<DynamicStateFlags> dynamicStateFlags{DynamicStateFlags::Viewport};

		return CreateAsync(
			logicalDevice,
			shaderCache,
			m_pipelineLayout,
			renderPass,
			vertexStage,
			primitiveInfo,
			viewports,
			scissors,
			subpassIndex,
			fragmentStage,
			Optional<const MultisamplingInfo*>{},
			depthStencil,
			Optional<const GeometryStageInfo*>{},
			dynamicStateFlags
		);
	}

	void FontGradientPipeline::Draw(
		Rendering::LogicalDevice& logicalDevice,
		const Rendering::RenderCommandEncoderView renderCommandEncoder,
		const Font::Atlas& __restrict fontAtlas,
		const Rendering::DescriptorSetView descriptorSet,
		const ConstUnicodeStringView text,
		Math::Vector2f positionRatio,
		const Math::Vector2f inverseRenderSizeRatio,
		const Math::Color color,
		const float depth,
		const Math::LinearGradient& gradient
	) const
	{
		uint32 tallestGlyphHeight = 0;

		for (const UnicodeCharType glyph : text)
		{
			Optional<const Font::Atlas::GlyphInfo*> pGlyphInfo = fontAtlas.GetGlyphInfo(glyph);
			Assert(pGlyphInfo.IsValid());

			if (LIKELY(pGlyphInfo.IsValid()))
			{
				tallestGlyphHeight = Math::Max(tallestGlyphHeight, pGlyphInfo->m_pixelSize.y);
			}
		}

		renderCommandEncoder.BindDescriptorSets(
			m_pipelineLayout,
			ArrayView<const Rendering::DescriptorSetView, uint8>(descriptorSet),
			GetFirstDescriptorSetIndex()
		);

		const Math::Vector2ui textSize = fontAtlas.CalculateSize(text);
		Math::Vector2f gradientOffset = Math::Zero;

		for (const UnicodeCharType glyph : text)
		{
			Optional<const Font::Atlas::GlyphInfo*> pGlyphInfo = fontAtlas.GetGlyphInfo(glyph);

			Assert(pGlyphInfo.IsValid());
			if (LIKELY(pGlyphInfo.IsValid()))
			{
				const Font::Atlas::GlyphInfo& glyphInfo = *pGlyphInfo;
				FontGradientConstants constants;
				constants.fragmentConstants.color = (Math::PackedColor32)color;
				constants.fragmentConstants.uvOrigin = glyphInfo.m_atlasCoordinates;
				constants.fragmentConstants.uvScale = glyphInfo.m_atlasScale;

				const Math::Vector2f offset = {(float)-glyphInfo.m_offset.x, (float)glyphInfo.m_offset.y - (float)tallestGlyphHeight};

				Math::Vector2f relativeGlyphSize = (Math::Vector2f)pGlyphInfo->m_pixelSize / (Math::Vector2f)textSize;
				constants.fragmentConstants.uvGradientScale = relativeGlyphSize;
				constants.fragmentConstants.uvGradientOffset = gradientOffset;
				gradientOffset = Math::Min(gradientOffset + relativeGlyphSize, Math::Vector2f(1.0f, 1.0f));

				constants.vertexConstants.positionRatio = positionRatio - (offset * inverseRenderSizeRatio * 2.f);
				constants.vertexConstants.sizeRatio = constants.vertexConstants.positionRatio +
				                                      (Math::Vector2f)glyphInfo.m_pixelSize * inverseRenderSizeRatio * 2.f;
				constants.vertexConstants.depth = depth;

				// Rotate clock-wise
				Math::Anglef orientation = Math::PI2 - gradient.m_orientation;
				uint8 counter = 0;

				Math::Vector2f line(0.0, 1.0f);
				Math::WorldRotation2D rotation(orientation);
				line = Math::Round(rotation.TransformDirection(line));
				for (Math::LinearGradient::Color gcolor : gradient.m_colors)
				{
					Math::Vector2f point(0.0f, gcolor.m_stopPoint);
					point = line * point.GetLength();
					if (line.x < 0.0f)
					{
						point.x += 1.0f;
					}
					if (line.y < 0.0f)
					{
						point.y += 1.0f;
					}

					constants.fragmentConstants.points[counter] = point;
					constants.fragmentConstants.colors[counter] = (Math::PackedColor32)gcolor.m_color;
					counter++;
				}

				PushConstants(logicalDevice, renderCommandEncoder, FontGradientPipelinePushConstantRanges, constants);

				renderCommandEncoder.Draw(6, 1);

				positionRatio += (Math::Vector2f)glyphInfo.m_advance * inverseRenderSizeRatio * 2.f;
			}
		}
	}
}
