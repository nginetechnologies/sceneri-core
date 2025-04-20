#include "Pipelines/ColoredIconPipeline.h"

#include <Renderer/Vulkan/Includes.h>

#include <Renderer/Devices/LogicalDevice.h>
#include <Renderer/Pipelines/PushConstantRange.h>
#include <Renderer/Descriptors/DescriptorSetView.h>

#include <Common/Math/Angle.h>
#include <Common/Math/Color.h>
#include <Common/Memory/OffsetOf.h>
#include <Common/Threading/Jobs/JobBatch.h>

namespace ngine::Rendering
{
	struct ColoredIconConstants
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
		};

		VertexConstants vertexConstants;
		FragmentConstants fragmentConstants;
	};

	inline static constexpr Array<const PushConstantRange, 2> ColoredIconPipelinePushConstantRanges = {
		PushConstantRange{
			ShaderStage::Fragment,
			static_cast<uint32>(OFFSET_OF(ColoredIconConstants, fragmentConstants)),
			sizeof(ColoredIconConstants::FragmentConstants)
		},
		PushConstantRange{
			ShaderStage::Vertex,
			static_cast<uint32>(OFFSET_OF(ColoredIconConstants, vertexConstants)),
			sizeof(ColoredIconConstants::VertexConstants)
		}
	};

	ColoredIconPipeline::ColoredIconPipeline(
		Rendering::LogicalDevice& logicalDevice, const ArrayView<const DescriptorSetLayoutView> descriptorSetLayouts
	)
	{
		CreateBase(logicalDevice, descriptorSetLayouts, ColoredIconPipelinePushConstantRanges);
	}

	Threading::JobBatch ColoredIconPipeline::CreatePipeline(
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
			ShaderStageInfo{"6c41b366-8c8e-db35-8567-df602cff4fdc"_asset},
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

	void ColoredIconPipeline::Draw(
		LogicalDevice& logicalDevice,
		const RenderCommandEncoderView renderCommandEncoder,
		const Math::Vector2f positionRatio,
		const Math::Vector2f sizeRatio,
		const Math::Anglef angle,
		const Math::Color color,
		const float depth,
		const DescriptorSetView imageDescriptorSet
	) const
	{
		ColoredIconConstants constants;
		constants.fragmentConstants.color = (Math::PackedColor32)color;
		constants.vertexConstants.positionRatio = positionRatio;
		constants.vertexConstants.sizeRatio = sizeRatio;
		constants.vertexConstants.angle = angle;
		constants.vertexConstants.depth = depth;

		PushConstants(logicalDevice, renderCommandEncoder, ColoredIconPipelinePushConstantRanges, constants);

		renderCommandEncoder
			.BindDescriptorSets(m_pipelineLayout, ArrayView<const DescriptorSetView, uint8>(imageDescriptorSet), GetFirstDescriptorSetIndex());
		renderCommandEncoder.Draw(6, 1);
	}
}
