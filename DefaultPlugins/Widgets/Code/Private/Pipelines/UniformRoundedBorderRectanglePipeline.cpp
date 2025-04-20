#include "Pipelines/UniformRoundedBorderRectanglePipeline.h"

#include <Renderer/Vulkan/Includes.h>

#include <Renderer/Devices/LogicalDevice.h>
#include <Renderer/Pipelines/PushConstantRange.h>

#include <Common/Math/Angle.h>
#include <Common/Math/Color.h>
#include <Common/Memory/OffsetOf.h>
#include <Common/Threading/Jobs/JobBatch.h>

namespace ngine::Rendering
{
	struct UniformRoundedBorderRectangleConstants
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
			Math::PackedColor32 color;
			float roundingRadius;
			float aspectRatio;
			Math::Vector2f innerSize;
		};

		VertexConstants vertexConstants;
		FragmentConstants fragmentConstants;
	};

	inline static constexpr Array<const PushConstantRange, 2> UniformRoundedBorderRectanglePushConstantRanges = {
		PushConstantRange{
			ShaderStage::Fragment,
			OFFSET_OF(UniformRoundedBorderRectangleConstants, fragmentConstants),
			sizeof(UniformRoundedBorderRectangleConstants::FragmentConstants)
		},
		PushConstantRange{
			ShaderStage::Vertex,
			OFFSET_OF(UniformRoundedBorderRectangleConstants, vertexConstants),
			sizeof(UniformRoundedBorderRectangleConstants::VertexConstants)
		}
	};

	UniformRoundedBorderRectanglePipeline::UniformRoundedBorderRectanglePipeline(Rendering::LogicalDevice& logicalDevice)
	{
		CreateBase(logicalDevice, {}, UniformRoundedBorderRectanglePushConstantRanges);
	}

	Threading::JobBatch UniformRoundedBorderRectanglePipeline::CreatePipeline(
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
			ShaderStageInfo{"0142582d-9aa5-4644-b921-87edbbee3f2b"_asset},
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

	void UniformRoundedBorderRectanglePipeline::Draw(
		LogicalDevice& logicalDevice,
		const RenderCommandEncoderView renderCommandEncoder,
		const Math::Vector2f positionRatio,
		Math::Vector2f sizeRatio,
		const Math::Anglef angle,
		const float depth,
		const Math::Color color,
		const float roundingRadius,
		const float aspectRatio,
		const Math::Vector2f innerSize
	) const
	{
		UniformRoundedBorderRectangleConstants constants;
		constants.vertexConstants.positionRatio = positionRatio;
		constants.vertexConstants.sizeRatio = sizeRatio;
		constants.vertexConstants.angle = angle;
		constants.vertexConstants.depth = depth;

		constants.fragmentConstants.color = (Math::PackedColor32)color;
		constants.fragmentConstants.roundingRadius = roundingRadius;
		constants.fragmentConstants.aspectRatio = aspectRatio;
		constants.fragmentConstants.innerSize = innerSize;

		PushConstants(logicalDevice, renderCommandEncoder, UniformRoundedBorderRectanglePushConstantRanges, constants);
		renderCommandEncoder.Draw(6, 1);
	}
}
