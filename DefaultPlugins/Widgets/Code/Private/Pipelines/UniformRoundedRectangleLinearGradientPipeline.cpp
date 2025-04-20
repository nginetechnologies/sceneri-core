#include "Pipelines/UniformRoundedRectangleLinearGradientPipeline.h"
#include "Pipelines/GradientCommon.h"

#include <Renderer/Vulkan/Includes.h>

#include <Renderer/Devices/LogicalDevice.h>
#include <Renderer/Pipelines/PushConstantRange.h>

#include <Common/Math/Color.h>
#include <Common/Memory/OffsetOf.h>
#include <Common/Threading/Jobs/JobBatch.h>

namespace ngine::Rendering
{
	struct UniformRoundedGradientRectangleConstants
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
			float roundingRadius;
			float aspectRatio;
			Math::Vector2f point[3];
		};

		VertexConstants vertexConstants;
		FragmentConstants fragmentConstants;
	};

	inline static constexpr Array<const PushConstantRange, 2> UniformRoundedRectangleGradientPushConstantRanges = {
		PushConstantRange{
			ShaderStage::Fragment,
			OFFSET_OF(UniformRoundedGradientRectangleConstants, fragmentConstants),
			sizeof(UniformRoundedGradientRectangleConstants::FragmentConstants)
		},
		PushConstantRange{
			ShaderStage::Vertex,
			OFFSET_OF(UniformRoundedGradientRectangleConstants, vertexConstants),
			sizeof(UniformRoundedGradientRectangleConstants::VertexConstants)
		}
	};

	UniformRoundedRectangleLinearGradientPipeline::UniformRoundedRectangleLinearGradientPipeline(Rendering::LogicalDevice& logicalDevice)
	{
		CreateBase(logicalDevice, {}, UniformRoundedRectangleGradientPushConstantRanges);
	}

	Threading::JobBatch UniformRoundedRectangleLinearGradientPipeline::CreatePipeline(
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
			ShaderStageInfo{"32272324-91fc-4061-8833-b15cc207e0e6"_asset},
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

	void UniformRoundedRectangleLinearGradientPipeline::Draw(
		LogicalDevice& logicalDevice,
		const RenderCommandEncoderView renderCommandEncoder,
		const Math::Vector2f positionRatio,
		Math::Vector2f sizeRatio,
		const Math::Anglef angle,
		const float depth,
		const Math::Color color,
		const float roundingRadius,
		const float aspectRatio,
		const Math::LinearGradient& gradient
	) const
	{
		UniformRoundedGradientRectangleConstants constants;
		constants.vertexConstants.positionRatio = positionRatio;
		constants.vertexConstants.sizeRatio = sizeRatio;
		constants.vertexConstants.angle = angle;
		constants.vertexConstants.depth = depth;

		constants.fragmentConstants.solidColor = (Math::PackedColor32)color;
		constants.fragmentConstants.roundingRadius = roundingRadius;
		constants.fragmentConstants.aspectRatio = aspectRatio;

		constants.fragmentConstants.solidColor = (Math::PackedColor32) "#FFFFFF"_color;

		Array<Math::Color, 3> gradientColors;
		RotateGradient(gradient, gradientColors, constants.fragmentConstants.point);
		for (uint8 i = 0; i < 3; ++i)
		{
			constants.fragmentConstants.color[i] = (Math::PackedColor32)gradientColors[i];
		}

		PushConstants(logicalDevice, renderCommandEncoder, UniformRoundedRectangleGradientPushConstantRanges, constants);
		renderCommandEncoder.Draw(6, 1);
	}
}
