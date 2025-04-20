#include "Pipelines/CircleConicGradientPipeline.h"
#include "Pipelines/GradientCommon.h"

#include <Renderer/Vulkan/Includes.h>

#include <Renderer/Devices/LogicalDevice.h>
#include <Renderer/Pipelines/PushConstantRange.h>

#include <Common/Math/Color.h>
#include <Common/Memory/OffsetOf.h>
#include <Common/Threading/Jobs/JobBatch.h>

namespace ngine::Rendering
{
	struct CircleConicGradientConstants
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
			Math::PackedColor32 color[8];
			Math::Ratiof stopPoint[9];
		};

		VertexConstants vertexConstants;
		FragmentConstants fragmentConstants;
	};

	inline static constexpr Array<const PushConstantRange, 2> CircleConicGradientPipelinePushConstantRanges = {
		PushConstantRange{
			ShaderStage::Fragment,
			static_cast<uint32>(OFFSET_OF(CircleConicGradientConstants, fragmentConstants)),
			sizeof(CircleConicGradientConstants::FragmentConstants)
		},
		PushConstantRange{
			ShaderStage::Vertex,
			static_cast<uint32>(OFFSET_OF(CircleConicGradientConstants, vertexConstants)),
			sizeof(CircleConicGradientConstants::VertexConstants)
		}
	};

	CircleConicGradientPipeline::CircleConicGradientPipeline(Rendering::LogicalDevice& logicalDevice)
	{
		CreateBase(logicalDevice, {}, CircleConicGradientPipelinePushConstantRanges);
	}

	Threading::JobBatch CircleConicGradientPipeline::CreatePipeline(
		LogicalDevice& logicalDevice,
		ShaderCache& shaderCache,
		const RenderPassView renderPass,
		const Math::Rectangleui outputArea,
		const Math::Rectangleui renderArea,
		const uint8 subpassIndex
	)
	{
		const VertexStageInfo vertexStage{ShaderStageInfo{"AC5838F2-D076-4CFA-8101-87A7E99E9FE9"_asset}};

		const PrimitiveInfo primitiveInfo{PrimitiveTopology::TriangleList, PolygonMode::Fill, WindingOrder::CounterClockwise, CullMode::Front};

		const Array<Viewport, 1> viewports{Viewport{outputArea}};
		const Array<Math::Rectangleui, 1> scissors{renderArea};

		const ColorTargetInfo colorBlendAttachment{
			ColorAttachmentBlendState{BlendFactor::SourceAlpha, BlendFactor::OneMinusSourceAlpha, BlendOperation::Add},
			AlphaAttachmentBlendState{BlendFactor::One, BlendFactor::One, BlendOperation::Maximum}
		};

		const FragmentStageInfo fragmentStage{
			ShaderStageInfo{"e69162f5-2177-4cb4-ab4c-c807e1dc27a2"_asset},
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

	void CircleConicGradientPipeline::Draw(
		LogicalDevice& logicalDevice,
		const RenderCommandEncoderView renderCommandEncoder,
		const Math::Vector2f positionRatio,
		Math::Vector2f sizeRatio,
		const Math::Anglef angle,
		const float depth,
		const Math::LinearGradient& gradient
	) const
	{
		CircleConicGradientConstants constants;
		constants.vertexConstants.positionRatio = positionRatio;
		constants.vertexConstants.sizeRatio = sizeRatio;
		constants.vertexConstants.angle = angle;
		constants.vertexConstants.depth = depth;

		constants.fragmentConstants.solidColor = (Math::PackedColor32) "#FFFFFF"_color;
		constants.fragmentConstants.stopPoint[0] = gradient.m_orientation.GetRadians();
		uint8 index = 0;
		for (const Math::LinearGradient::Color color : gradient.m_colors)
		{
			constants.fragmentConstants.color[index] = (Math::PackedColor32)color.m_color;
			constants.fragmentConstants.stopPoint[index + 1] = color.m_stopPoint;
			++index;
		}

		PushConstants(logicalDevice, renderCommandEncoder, CircleConicGradientPipelinePushConstantRanges, constants);
		renderCommandEncoder.Draw(6, 1);
	}
}
