#include "Pipelines/LinePipeline.h"

#include <Renderer/Vulkan/Includes.h>

#include <Renderer/Devices/LogicalDevice.h>
#include <Renderer/Pipelines/PushConstantRange.h>

#include <Common/Math/Primitives/Spline.h>
#include <Common/Math/Vector2.h>
#include <Common/Memory/OffsetOf.h>
#include <Common/Threading/Jobs/JobBatch.h>

namespace ngine::Rendering
{
#define MAXIMUM_POINT_COUNT 4

	struct LineConstants
	{
		struct VertexConstants
		{
			Math::Vector2f positionRatio;
			Math::Vector2f sizeRatio;
			Math::Anglef angle;
			float depth;
		};

		struct FragmentConstants
		{
			Math::Color lineColor;
			Math::Vector2f points[MAXIMUM_POINT_COUNT];
			uint32 pointCount;
			float aspectRatio;
			float thickness;
		};

		VertexConstants vertexConstants;
		FragmentConstants fragmentConstants;
	};

	inline static constexpr Array<const PushConstantRange, 2> LinePushConstantRanges = {
		PushConstantRange{ShaderStage::Fragment, OFFSET_OF(LineConstants, fragmentConstants), sizeof(LineConstants::FragmentConstants)},
		PushConstantRange{ShaderStage::Vertex, OFFSET_OF(LineConstants, vertexConstants), sizeof(LineConstants::VertexConstants)}
	};

	LinePipeline::LinePipeline(Rendering::LogicalDevice& logicalDevice)
	{
		CreateBase(logicalDevice, {}, LinePushConstantRanges);
	}

	Threading::JobBatch LinePipeline::CreatePipeline(
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
			ShaderStageInfo{"606f4275-485c-4626-aad1-ac51f7edc5cf"_asset},
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

	void LinePipeline::Draw(
		LogicalDevice& logicalDevice,
		const RenderCommandEncoderView renderCommandEncoder,
		const Math::Vector2f positionRatio,
		Math::Vector2f sizeRatio,
		const Math::Anglef angle,
		const float depth,
		const Math::Spline2f& spline,
		const Math::Color lineColor,
		const float thickness,
		const float aspectRatio
	) const
	{
		LineConstants constants{LineConstants::VertexConstants{positionRatio, sizeRatio, angle, depth}, LineConstants::FragmentConstants{}};

		constants.fragmentConstants.lineColor = lineColor;

		Assert(spline.GetPointCount() <= MAXIMUM_POINT_COUNT);
		uint32 index = 0;
		for (const Math::Spline2f::Point& splinePoint : spline.GetPoints())
		{
			constants.fragmentConstants.points[index++] = splinePoint.position;
		}
		constants.fragmentConstants.pointCount = spline.GetPointCount();

		constants.fragmentConstants.aspectRatio = aspectRatio;
		constants.fragmentConstants.thickness = thickness;

		PushConstants(logicalDevice, renderCommandEncoder, LinePushConstantRanges, constants);
		renderCommandEncoder.Draw(6, 1);
	}
}
