#pragma once

#include "GraphicsPipelineBase.h"
#include "CullMode.h"

#include <Renderer/Format.h>

#include <Renderer/Pipelines/AttachmentBlendState.h>
#include <Renderer/Commands/RenderCommandEncoderView.h>
#include <Renderer/Wrappers/GraphicsPipelineView.h>
#include <Renderer/Wrappers/CompareOperation.h>
#include <Renderer/Wrappers/StencilOperation.h>

#include <Renderer/SampleCount.h>
#include <Renderer/Wrappers/RenderPassView.h>
#include <Renderer/ColorChannel.h>
#include <Renderer/ShaderStage.h>

#include <Common/Memory/ReferenceWrapper.h>
#include <Common/Memory/Containers/ArrayView.h>
#include <Common/Memory/Containers/ByteView.h>
#include <Common/Math/Primitives/Rectangle.h>
#include <Common/Math/Log2.h>
#include <Common/EnumFlags.h>
#include <Common/EnumFlagOperators.h>

namespace ngine::Threading
{
	struct JobBatch;
}

namespace ngine::Rendering
{
	enum class VertexInputRate : uint8
	{
		Vertex = 0,
		Instance = 1
	};

	struct VertexInputBindingDescription
	{
		uint32 binding;
		uint32 stride;
		VertexInputRate inputRate;
	};

	struct VertexInputAttributeDescription
	{
		uint32 shaderLocation;
		uint32 binding;
		Format format;
		uint32 offset;
	};

	struct VertexStageInfo : public ShaderStageInfo
	{
		ArrayView<const VertexInputBindingDescription, uint8> m_bindingDescriptions;
		ArrayView<const VertexInputAttributeDescription, uint8> m_attributeDescriptions;
	};

	enum class PolygonMode : uint8
	{
		Fill = 0,
		Line = 1,
		Point = 2
	};

	enum class PrimitiveTopology : uint32
	{
		TriangleList = 3
	};

	enum class PrimitiveFlags : uint8
	{
		DepthClamp = 1 << 0,
		DepthBias = 1 << 1,
		//! Whether primitives are discarded immediately before the rasterization stage.
		RasterizerDiscard = 1 << 2
	};
	ENUM_FLAG_OPERATORS(PrimitiveFlags);

	struct PrimitiveInfo
	{
		PrimitiveTopology topology = PrimitiveTopology::TriangleList;
		PolygonMode polygonMode = PolygonMode::Fill;
		WindingOrder windingOrder = WindingOrder::CounterClockwise;
		EnumFlags<CullMode> cullMode;
		EnumFlags<PrimitiveFlags> flags;
		int depthBiasConstantFactor{0};
		float depthBiasClamp{0.f};
		float depthBiasSlopeFactor{0.f};
	};

	struct ColorTargetInfo
	{
		ColorTargetInfo() = default;
		ColorTargetInfo(
			const ColorAttachmentBlendState colorAttachmentBlendState,
			const AlphaAttachmentBlendState alphaAttachmentBlendState,
			const EnumFlags<ColorChannel> writtenChannels = ColorChannel::All
		)
			: colorBlendState(colorAttachmentBlendState)
			, alphaBlendState(alphaAttachmentBlendState)
			, colorWriteMask(writtenChannels)
		{
		}
		ColorTargetInfo(
			const ColorAttachmentBlendState colorAttachmentBlendState, const EnumFlags<ColorChannel> writtenChannels = ColorChannel::RGB
		)
			: colorBlendState(colorAttachmentBlendState)
			, colorWriteMask(writtenChannels)
		{
		}
		ColorTargetInfo(
			const AlphaAttachmentBlendState alphaAttachmentBlendState, const EnumFlags<ColorChannel> writtenChannels = ColorChannel::Alpha
		)
			: alphaBlendState(alphaAttachmentBlendState)
			, colorWriteMask(writtenChannels)
		{
		}

		ColorAttachmentBlendState colorBlendState{BlendFactor::Zero, BlendFactor::Zero, BlendOperation::Add};
		AlphaAttachmentBlendState alphaBlendState{BlendFactor::Zero, BlendFactor::Zero, BlendOperation::Add};
		EnumFlags<ColorChannel> colorWriteMask{ColorChannel::All};
	};

	struct FragmentStageInfo : public ShaderStageInfo
	{
		ArrayView<const ColorTargetInfo, uint8> m_colorTargets;
	};

	struct Viewport
	{
		Viewport(const Math::Rectanglef area, const float minimumDepth = 0.f, const float maximumDepth = 1.f)
			: m_area(area)
			, minDepth(minimumDepth)
			, maxDepth(maximumDepth)
		{
		}
		Viewport(const Math::Rectangleui area, const float minimumDepth = 0.f, const float maximumDepth = 1.f)
			: Viewport((Math::Rectanglef)area, minimumDepth, maximumDepth)
		{
		}

		Math::Rectanglef m_area;
		float minDepth;
		float maxDepth;
	};

	struct MultisamplingInfo
	{
		SampleCount rasterizationSamples{SampleCount::One};
	};

	struct StencilOperationState
	{
		StencilOperation failOp;
		StencilOperation passOp;
		StencilOperation depthFailOp;
		CompareOperation compareOp;
		uint32 compareMask;
		uint32 writeMask;
		uint32 reference;
	};

	enum class DepthStencilFlags : uint8
	{
		None = 0,
		DepthTest = 1 << 0,
		DepthWrite = 1 << 1,
		DepthBoundsTest = 1 << 2,
		StencilTest = 1 << 3
	};
	ENUM_FLAG_OPERATORS(DepthStencilFlags);

	struct DepthStencilInfo
	{
		DepthStencilInfo(
			const EnumFlags<DepthStencilFlags> flags,
			const CompareOperation depthComparison,
			StencilOperationState&& frontState =
				{StencilOperation::Keep, StencilOperation::Keep, StencilOperation::Keep, CompareOperation::AlwaysSucceed},
			StencilOperationState&& backState =
				{StencilOperation::Keep, StencilOperation::Keep, StencilOperation::Keep, CompareOperation::AlwaysSucceed}
		)
			: m_flags(flags)
			, m_depthCompareOperation(depthComparison)
			, m_front(Forward<StencilOperationState>(frontState))
			, m_back(Forward<StencilOperationState>(backState))
		{
		}

		EnumFlags<DepthStencilFlags> m_flags;
		CompareOperation m_depthCompareOperation{CompareOperation::AlwaysFail};
		StencilOperationState m_front{StencilOperation::Keep, StencilOperation::Keep, StencilOperation::Keep, CompareOperation::AlwaysSucceed};
		StencilOperationState m_back{StencilOperation::Keep, StencilOperation::Keep, StencilOperation::Keep, CompareOperation::AlwaysSucceed};
	};

	struct GeometryStageInfo : public ShaderStageInfo
	{
	};

	enum class DynamicStateFlags : uint16
	{
		Viewport = 1 << 0,
		Scissor = 1 << 1,
		LineWidth = 1 << 2,
		DepthBias = 1 << 3,
		BlendConstants = 1 << 4,
		DepthBounds = 1 << 5,
		StencilCompareMask = 1 << 6,
		StencilWriteMask = 1 << 7,
		StencilReference = 1 << 8,
		CullMode = 1 << 9,
		Last = CullMode,
		Count = Math::Log2((uint32)Last) + 1,
	};
	ENUM_FLAG_OPERATORS(DynamicStateFlags);

	struct GraphicsPipeline : public GraphicsPipelineBase
	{
		GraphicsPipeline() = default;
		GraphicsPipeline(const GraphicsPipeline&) = delete;
		GraphicsPipeline(GraphicsPipeline&& other) = default;
		GraphicsPipeline& operator=(const GraphicsPipeline& other) = delete;
		GraphicsPipeline& operator=(GraphicsPipeline&& other);

		void Destroy(LogicalDevice& logicalDevice);

		void CreateBase(
			LogicalDevice& logicalDevice,
			const ArrayView<const DescriptorSetLayoutView, uint8> descriptorSetLayouts = {},
			const ArrayView<const PushConstantRange, uint8> pushConstantRanges = {}
		);

		[[nodiscard]] Threading::JobBatch CreateAsync(
			LogicalDevice& logicalDevice,
			ShaderCache& shaderCache,
			const PipelineLayoutView pipelineLayout,
			const RenderPassView renderPass,
			const VertexStageInfo& vertexStage,
			const PrimitiveInfo& primitiveInfo = PrimitiveInfo{},
			ArrayView<const Viewport> viewports = {},
			ArrayView<const Math::Rectangleui> scissors = {},
			const uint8 subpassIndex = 0,
			Optional<const FragmentStageInfo*> pFragmentStageInfo = Invalid,
			Optional<const MultisamplingInfo*> pMultisamplingInfo = Invalid,
			Optional<const DepthStencilInfo*> pDepthStencilInfo = Invalid,
			Optional<const GeometryStageInfo*> pGeometryStageInfo = Invalid,
			EnumFlags<DynamicStateFlags> dynamicStateFlags = {}
		);

		[[nodiscard]] bool IsValid() const
		{
			return m_pipeline.IsValid();
		}
		[[nodiscard]] operator GraphicsPipelineView() const
		{
			return m_pipeline;
		}

		void PrepareForResize(const LogicalDeviceView logicalDevice);

		template<typename Type>
		void PushConstants(
			LogicalDevice& logicalDevice,
			const RenderCommandEncoderView renderCommandEncoder,
			const ArrayView<const PushConstantRange, uint8> pushConstantRanges,
			const Type& data
		) const
		{
			PushConstants(logicalDevice, renderCommandEncoder, pushConstantRanges, ConstByteView::Make(data));
		}
	protected:
		void CreateInternal(
			LogicalDevice& logicalDevice,
			ShaderCache& shaderCache,
			const PipelineLayoutView pipelineLayout,
			const RenderPassView renderPass,
			const VertexStageInfo& vertexStage,
			const PrimitiveInfo& primitiveInfo = PrimitiveInfo{},
			ArrayView<const Viewport> viewports = {},
			ArrayView<const Math::Rectangleui> scissors = {},
			const uint8 subpassIndex = 0,
			Optional<const FragmentStageInfo*> pFragmentStageInfo = Invalid,
			Optional<const MultisamplingInfo*> pMultisamplingInfo = Invalid,
			Optional<const DepthStencilInfo*> pDepthStencilInfo = Invalid,
			Optional<const GeometryStageInfo*> pGeometryStageInfo = Invalid,
			EnumFlags<DynamicStateFlags> dynamicStateFlags = {}
		);

		void PushConstants(
			LogicalDevice& logicalDevice,
			const RenderCommandEncoderView renderCommandEncoder,
			const ArrayView<const PushConstantRange, uint8> pushConstantRanges,
			const ConstByteView data
		) const;

		void SetDebugName(const LogicalDevice& logicalDevice, const ConstZeroTerminatedStringView name);
	protected:
		friend RenderCommandEncoderView;

		GraphicsPipelineView m_pipeline;

#if RENDERER_METAL
		id<MTLDepthStencilState> m_depthStencilState;
		EnumFlags<CullMode> m_cullMode = CullMode::None;
		WindingOrder m_windingOrder = WindingOrder::CounterClockwise;
		PolygonMode m_polygonMode = PolygonMode::Fill;
		bool m_depthClamp = false;
		uint32 m_stencilFrontReference{0};
		uint32 m_stencilBackReference{0};
		int m_depthBiasConstantFactor{0};
		float m_depthBiasSlopeFactor{0.f};
		float m_depthBiasClamp{0.f};
#elif RENDERER_WEBGPU
		bool m_stencilTest{false};
		uint32 m_stencilReference{0};
#endif
	};
}
