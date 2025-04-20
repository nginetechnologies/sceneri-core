#include "FontPipeline.h"

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
	struct FontConstants
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
		};

		VertexConstants vertexConstants;
		FragmentConstants fragmentConstants;
	};

	inline static constexpr Array<const Rendering::PushConstantRange, 2> FontPipelinePushConstantRanges = {
		Rendering::PushConstantRange{
			Rendering::ShaderStage::Vertex, static_cast<uint32>(OFFSET_OF(FontConstants, vertexConstants)), sizeof(FontConstants::VertexConstants)
		},
		Rendering::PushConstantRange{
			Rendering::ShaderStage::Fragment,
			static_cast<uint32>(OFFSET_OF(FontConstants, fragmentConstants)),
			sizeof(FontConstants::FragmentConstants)
		}
	};

	FontPipeline::FontPipeline(
		Rendering::LogicalDevice& logicalDevice, const ArrayView<const Rendering::DescriptorSetLayoutView> descriptorSetLayouts
	)
	{
		CreateBase(logicalDevice, descriptorSetLayouts, FontPipelinePushConstantRanges);
	}

	Threading::JobBatch FontPipeline::CreatePipeline(
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
			ShaderStageInfo{"58B32F46-5045-496E-8EBE-267473353F3E"_asset},
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

	void FontPipeline::Draw(
		Rendering::LogicalDevice& logicalDevice,
		const Rendering::RenderCommandEncoderView renderCommandEncoder,
		const Font::Atlas& __restrict fontAtlas,
		const Rendering::DescriptorSetView descriptorSet,
		const ConstUnicodeStringView text,
		Math::Vector2f positionRatio,
		const Math::Vector2f inverseRenderSizeRatio,
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
			if (LIKELY(pGlyphInfo.IsValid()))
			{
				const Font::Atlas::GlyphInfo& glyphInfo = *pGlyphInfo;

				FontConstants constants;
				constants.fragmentConstants.color = (Math::PackedColor32)color;
				constants.fragmentConstants.uvOrigin = glyphInfo.m_atlasCoordinates;
				constants.fragmentConstants.uvScale = glyphInfo.m_atlasScale;

				const Math::Vector2f offset = {(float)-glyphInfo.m_offset.x, (float)glyphInfo.m_offset.y - (float)tallestGlyphHeight};

				constants.vertexConstants.positionRatio = positionRatio - (offset * inverseRenderSizeRatio * 2.f);
				constants.vertexConstants.sizeRatio = constants.vertexConstants.positionRatio +
				                                      (Math::Vector2f)glyphInfo.m_pixelSize * inverseRenderSizeRatio * 2.f;
				constants.vertexConstants.depth = depth;

				PushConstants(logicalDevice, renderCommandEncoder, FontPipelinePushConstantRanges, constants);

				renderCommandEncoder.Draw(6, 1);

				positionRatio += (Math::Vector2f)glyphInfo.m_advance * inverseRenderSizeRatio * 2.f;
			}
		}
	}
}
