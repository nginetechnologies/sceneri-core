#pragma once

#include "GraphicsPipelineBase.h"

#include <Renderer/Commands/ComputeCommandEncoderView.h>
#include <Renderer/Wrappers/ComputePipelineView.h>

#include <Common/Memory/Containers/ByteView.h>
#include <Common/Math/Vector2.h>
#include <Common/Math/Vector3.h>

namespace ngine::Threading
{
	struct JobBatch;
}

namespace ngine::Rendering
{
	struct ComputePipeline : public GraphicsPipelineBase
	{
		ComputePipeline() = default;
		ComputePipeline(const ComputePipeline&) = delete;
		ComputePipeline(ComputePipeline&& other)
			: GraphicsPipelineBase(Forward<GraphicsPipelineBase>(other))
			, m_pipeline(other.m_pipeline)
		{
			other.m_pipeline = {};
		}
		ComputePipeline& operator=(const ComputePipeline& other) = delete;
		ComputePipeline& operator=(ComputePipeline&& other);

		void Destroy(LogicalDevice& logicalDevice);

		void CreateBase(
			LogicalDevice& logicalDevice,
			const ArrayView<const DescriptorSetLayoutView, uint8> descriptorSetLayouts = {},
			const ArrayView<const PushConstantRange, uint8> pushConstantRanges = {}
		);

		[[nodiscard]] Threading::JobBatch CreateAsync(
			const LogicalDeviceView logicalDevice, ShaderCache& shaderCache, const ShaderStageInfo stageInfo, const PipelineLayoutView layout
		);
		void Create(
			const LogicalDeviceView logicalDevice, ShaderCache& shaderCache, const ShaderStageInfo stageInfo, const PipelineLayoutView layout
		);

		[[nodiscard]] bool IsValid() const
		{
			return m_pipeline.IsValid();
		}
		[[nodiscard]] operator ComputePipelineView() const
		{
			return m_pipeline;
		}

		template<typename Type>
		void PushConstants(
			LogicalDevice& logicalDevice,
			const ComputeCommandEncoderView computeCommandEncoder,
			const ArrayView<const PushConstantRange, uint8> pushConstantRanges,
			const Type& data
		) const
		{
			PushConstants(logicalDevice, computeCommandEncoder, pushConstantRanges, ConstByteView::Make(data));
		}

		[[nodiscard]] PURE_NOSTATICS static constexpr Math::Vector3ui
		GetNumberOfThreadGroups(const Math::Vector3ui threadCount, const Math::Vector3ui threadGroupSize)
		{
			return (threadCount + threadGroupSize - Math::Vector3ui{1}) / threadGroupSize;
		}
		[[nodiscard]] PURE_NOSTATICS static constexpr Math::Vector3ui
		GetNumberOfThreadGroups(const Math::Vector2ui threadCount, const Math::Vector3ui threadGroupSize)
		{
			return GetNumberOfThreadGroups({threadCount.x, threadCount.y, 1}, threadGroupSize);
		}

		void SetDebugName(const LogicalDevice& logicalDevice, const ConstZeroTerminatedStringView name);
	protected:
		void CreateInternal(
			const LogicalDeviceView logicalDevice, ShaderCache& shaderCache, const ShaderStageInfo stageInfo, const PipelineLayoutView layout
		);

		void PushConstants(
			LogicalDevice& logicalDevice,
			const ComputeCommandEncoderView computeCommandEncoder,
			const ArrayView<const PushConstantRange, uint8> pushConstantRanges,
			const ConstByteView data
		) const;
	protected:
		ComputePipelineView m_pipeline;
	};
}
