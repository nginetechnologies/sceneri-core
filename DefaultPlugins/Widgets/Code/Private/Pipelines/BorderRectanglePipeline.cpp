#include "Pipelines/BorderRectanglePipeline.h"

#include <Renderer/Vulkan/Includes.h>

#include <Renderer/Devices/LogicalDevice.h>
#include <Renderer/Pipelines/PushConstantRange.h>

#include <Common/Math/Angle.h>
#include <Common/Math/Color.h>
#include <Common/Memory/OffsetOf.h>
#include <Common/Threading/Jobs/JobBatch.h>

namespace ngine::Rendering
{
	struct BorderRectangleConstants
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
			Math::Vector2f innerSize;
		};

		VertexConstants vertexConstants;
		FragmentConstants fragmentConstants;
	};

	inline static constexpr Array<const PushConstantRange, 2> BorderRectanglePushConstantRanges = {
		PushConstantRange{
			ShaderStage::Fragment, OFFSET_OF(BorderRectangleConstants, fragmentConstants), sizeof(BorderRectangleConstants::FragmentConstants)
		},
		PushConstantRange{
			ShaderStage::Vertex, OFFSET_OF(BorderRectangleConstants, vertexConstants), sizeof(BorderRectangleConstants::VertexConstants)
		}
	};

	BorderRectanglePipeline::BorderRectanglePipeline(Rendering::LogicalDevice& logicalDevice)
	{
		CreateBase(logicalDevice, {}, BorderRectanglePushConstantRanges);
	}

	Threading::JobBatch BorderRectanglePipeline::CreatePipeline(
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
			ShaderStageInfo{"65244a79-6d27-4c93-abf3-2509dc1441fe"_asset},
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

	void BorderRectanglePipeline::Draw(
		LogicalDevice& logicalDevice,
		const RenderCommandEncoderView renderCommandEncoder,
		const Math::Vector2f positionRatio,
		Math::Vector2f sizeRatio,
		const Math::Anglef angle,
		const float depth,
		const Math::Color color,
		const Math::Vector2f innerSize
	) const
	{
		BorderRectangleConstants constants;
		constants.fragmentConstants.color = (Math::PackedColor32)color;
		constants.fragmentConstants.innerSize = innerSize;
		constants.vertexConstants.positionRatio = positionRatio;
		constants.vertexConstants.sizeRatio = sizeRatio;
		constants.vertexConstants.angle = angle;
		constants.vertexConstants.depth = depth;

		PushConstants(logicalDevice, renderCommandEncoder, BorderRectanglePushConstantRanges, constants);
		renderCommandEncoder.Draw(6, 1);
	}
}
