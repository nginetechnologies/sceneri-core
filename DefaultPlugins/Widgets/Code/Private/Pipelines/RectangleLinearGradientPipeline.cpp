#include "Pipelines/RectangleLinearGradientPipeline.h"
#include "Pipelines/GradientCommon.h"

#include <Renderer/Vulkan/Includes.h>

#include <Renderer/Devices/LogicalDevice.h>
#include <Renderer/Pipelines/PushConstantRange.h>

#include <Common/Memory/OffsetOf.h>
#include <Common/Threading/Jobs/JobBatch.h>

namespace ngine::Rendering
{
	struct GradientRectangleConstants
	{
		struct VertexConstants
		{
			Math::Vector2f positionRatio;
			Math::Vector2f sizeRatio;
			Math::Anglef angle;
			float depth;
		};

		struct alignas(16) FragmentConstants
		{
			Math::PackedColor32 solidColor;
			// TODO: This should grow dynamically based on the gradient in question
			Math::PackedColor32 color[3];
			Math::Vector2f point[3];
		};

		VertexConstants vertexConstants;
		FragmentConstants fragmentConstants;
	};

	inline static constexpr Array<const PushConstantRange, 2> GradientPushConstantRanges = {
		PushConstantRange{
			ShaderStage::Fragment, OFFSET_OF(GradientRectangleConstants, fragmentConstants), sizeof(GradientRectangleConstants::FragmentConstants)
		},
		PushConstantRange{
			ShaderStage::Vertex, OFFSET_OF(GradientRectangleConstants, vertexConstants), sizeof(GradientRectangleConstants::VertexConstants)
		}
	};

	RectangleLinearGradientPipeline::RectangleLinearGradientPipeline(Rendering::LogicalDevice& logicalDevice)
	{
		CreateBase(logicalDevice, {}, GradientPushConstantRanges);
	}

	Threading::JobBatch RectangleLinearGradientPipeline::CreatePipeline(
		LogicalDevice& logicalDevice,
		ShaderCache& shaderCache,
		const RenderPassView renderPass,
		const Math::Rectangleui outputArea,
		const Math::Rectangleui renderArea,
		const uint8 subpassIndex
	)
	{
		const VertexStageInfo vertexStage{ShaderStageInfo{"ac5838f2-d076-4cfa-8101-87a7e99e9fe9"_asset}};

		const PrimitiveInfo primitiveInfo{PrimitiveTopology::TriangleList, PolygonMode::Fill, WindingOrder::CounterClockwise, CullMode::Front};

		const Array<Viewport, 1> viewports{Viewport{outputArea}};
		const Array<Math::Rectangleui, 1> scissors{renderArea};

		const ColorTargetInfo colorBlendAttachment{
			ColorAttachmentBlendState{BlendFactor::SourceAlpha, BlendFactor::OneMinusSourceAlpha, BlendOperation::Add},
			AlphaAttachmentBlendState{BlendFactor::One, BlendFactor::One, BlendOperation::Maximum}
		};
		const FragmentStageInfo fragmentStage{
			ShaderStageInfo{"8b08ed23-3c70-4a80-a4af-a43d41d2e9cd"_asset},
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

	void RectangleLinearGradientPipeline::Draw(
		LogicalDevice& logicalDevice,
		const RenderCommandEncoderView renderCommandEncoder,
		const Math::Vector2f positionRatio,
		Math::Vector2f sizeRatio,
		const Math::Anglef angle,
		const float depth,
		const Math::LinearGradient& gradient
	) const
	{
		Assert(gradient.m_colors.GetSize() <= 3, "Can not draw more than 3 color gradients at the moment");

		GradientRectangleConstants constants;
		constants.vertexConstants.positionRatio = positionRatio;
		constants.vertexConstants.sizeRatio = sizeRatio;
		constants.vertexConstants.angle = angle;
		constants.vertexConstants.depth = depth;

		constants.fragmentConstants.solidColor = (Math::PackedColor32) "#FFFFFF"_color;
		Array<Math::Color, 3> gradientColors;
		RotateGradient(gradient, gradientColors, constants.fragmentConstants.point);
		for (uint8 i = 0; i < 3; ++i)
		{
			constants.fragmentConstants.color[i] = (Math::PackedColor32)gradientColors[i];
		}

		PushConstants(logicalDevice, renderCommandEncoder, GradientPushConstantRanges, constants);
		renderCommandEncoder.Draw(6, 1);
	}
}
