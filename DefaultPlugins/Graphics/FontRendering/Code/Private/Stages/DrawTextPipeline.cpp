#include "Stages/DrawTextPipeline.h"

#include <Renderer/Vulkan/Includes.h>

#include <FontRendering/Font.h>
#include <FontRendering/FontAtlas.h>
#include <Renderer/Devices/LogicalDevice.h>
#include <Renderer/Pipelines/PushConstantRange.h>

#include <Common/Math/Color.h>
#include <Common/Memory/OffsetOf.h>
#include <Common/Memory/Containers/FlatVector.h>
#include <Common/Threading/Jobs/JobBatch.h>

namespace ngine::Font
{
	struct DrawTextConstants
	{
		struct FragmentConstants
		{
			Math::Color color;
			Math::Vector2f uvOrigin;
			Math::Vector2f uvScale;
		};

		struct VertexConstants
		{
			Math::Vector2f positionRatio;
			Math::Vector2f sizeRatio;
			float depth;
		};

		VertexConstants vertexConstants;
		FragmentConstants fragmentConstants;
	};

	inline static constexpr Array<const Rendering::DescriptorSetLayout::Binding, 2> DrawTextPipelineDescriptorBindings = {
		Rendering::DescriptorSetLayout::Binding::MakeSampledImage(
			0, Rendering::ShaderStage::Fragment, Rendering::SampledImageType::Float, Rendering::ImageMappingType::TwoDimensional
		),
		Rendering::DescriptorSetLayout::Binding::MakeSampler(1, Rendering::ShaderStage::Fragment, Rendering::SamplerBindingType::Filtering)
	};

	inline static constexpr Array<const Rendering::PushConstantRange, 2> DrawTextPipelinePushConstantRanges = {
		Rendering::PushConstantRange{
			Rendering::ShaderStage::Vertex,
			static_cast<uint32>(OFFSET_OF(DrawTextConstants, vertexConstants)),
			sizeof(DrawTextConstants::VertexConstants)
		},
		Rendering::PushConstantRange{
			Rendering::ShaderStage::Fragment,
			static_cast<uint32>(OFFSET_OF(DrawTextConstants, fragmentConstants)),
			sizeof(DrawTextConstants::FragmentConstants)
		}
	};

	DrawTextPipeline::DrawTextPipeline(Rendering::LogicalDevice& logicalDevice)
		: DescriptorSetLayout(logicalDevice, DrawTextPipelineDescriptorBindings)
	{
#if RENDERER_OBJECT_DEBUG_NAMES
		DescriptorSetLayout::SetDebugName(logicalDevice, "Draw Text");
#endif
		CreateBase(logicalDevice, ArrayView<const DescriptorSetLayoutView>{*this}, DrawTextPipelinePushConstantRanges);
	}

	void DrawTextPipeline::Destroy(Rendering::LogicalDevice& logicalDevice)
	{
		GraphicsPipeline::Destroy(logicalDevice);
		DescriptorSetLayout::Destroy(logicalDevice);
	}

	Threading::JobBatch DrawTextPipeline::CreatePipeline(
		Rendering::LogicalDevice& logicalDevice,
		Rendering::ShaderCache& shaderCache,
		const Rendering::RenderPassView renderPass,
		const Math::Rectangleui outputArea,
		const Math::Rectangleui renderArea,
		const uint8 subpassIndex
	)
	{
		using namespace Rendering;

		const VertexStageInfo vertexStage{ShaderStageInfo{"7AE3BFE2-5D04-4D3C-B09E-7F282452A40A"_asset}};

		const PrimitiveInfo primitiveInfo{PrimitiveTopology::TriangleList, PolygonMode::Fill, WindingOrder::CounterClockwise, CullMode::Front};

		const Array<Viewport, 1> viewports{Viewport{outputArea}};
		const Array<Math::Rectangleui, 1> scissors{renderArea};

		const ColorTargetInfo colorBlendAttachment{
			ColorAttachmentBlendState{BlendFactor::One, BlendFactor::OneMinusSourceAlpha, BlendOperation::Add},
			AlphaAttachmentBlendState{BlendFactor::One, BlendFactor::One, BlendOperation::Maximum}
		};
		const FragmentStageInfo fragmentStage{
			ShaderStageInfo{"58B32F46-5045-496E-8EBE-267473353F3E"_asset},
			ArrayView<const ColorTargetInfo>(colorBlendAttachment)
		};

		EnumFlags<DynamicStateFlags> dynamicStateFlags{};
		// TODO: Hardwired for VR / XR, change to pass in a flag instead
		dynamicStateFlags |= DynamicStateFlags::Viewport * (outputArea != renderArea);

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
			Optional<const DepthStencilInfo*>{},
			Optional<const GeometryStageInfo*>{},
			dynamicStateFlags
		);
	}

	void DrawTextPipeline::Draw(
		Rendering::LogicalDevice& logicalDevice,
		const Rendering::RenderCommandEncoderView renderCommandEncoder,
		const Font::Atlas& __restrict fontAtlas,
		const Rendering::DescriptorSetView descriptorSet,
		const ConstUnicodeStringView text,
		Math::Vector2f positionRatio,
		const Math::Vector2f renderSizeRatio,
		const Math::Color color,
		const float depth
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

		for (const UnicodeCharType glyph : text)
		{
			Optional<const Font::Atlas::GlyphInfo*> pGlyphInfo = fontAtlas.GetGlyphInfo(glyph);
			Assert(pGlyphInfo.IsValid());
			const Font::Atlas::GlyphInfo& glyphInfo = *pGlyphInfo;

			DrawTextConstants constants;
			constants.fragmentConstants.color = color;
			constants.fragmentConstants.uvOrigin = glyphInfo.m_atlasCoordinates;
			constants.fragmentConstants.uvScale = glyphInfo.m_atlasScale;

			const Math::Vector2f offset = {(float)-glyphInfo.m_offset.x, (float)glyphInfo.m_offset.y - (float)tallestGlyphHeight};

			constants.vertexConstants.positionRatio = positionRatio - (offset * renderSizeRatio * 2.f);
			constants.vertexConstants.sizeRatio = constants.vertexConstants.positionRatio +
			                                      (Math::Vector2f)glyphInfo.m_pixelSize * renderSizeRatio * 2.f;
			constants.vertexConstants.depth = depth;

			PushConstants(logicalDevice, renderCommandEncoder, DrawTextPipelinePushConstantRanges, constants);

			renderCommandEncoder.Draw(6, 1);

			positionRatio += (Math::Vector2f)glyphInfo.m_advance * renderSizeRatio * 2.f;
		}
	}
}
