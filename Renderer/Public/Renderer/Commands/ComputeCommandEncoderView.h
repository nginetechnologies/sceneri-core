#pragma once

#include <Common/Memory/Containers/ForwardDeclarations/ArrayView.h>
#include <Common/Platform/ForceInline.h>
#include <Common/Platform/TrivialABI.h>

#include <Renderer/Constants.h>
#include <Renderer/Vulkan/ForwardDeclares.h>
#include <Renderer/Metal/ForwardDeclares.h>
#include <Renderer/WebGPU/ForwardDeclares.h>

#include <Common/Memory/Containers/ZeroTerminatedStringView.h>
#include <Common/Math/Color.h>

#include <Common/Math/ForwardDeclarations/Vector3.h>

namespace ngine::Rendering
{
	struct LogicalDevice;
	struct CommandBufferView;
	struct BufferView;
	struct PipelineLayoutView;
	struct ComputePipeline;

	struct TRIVIAL_ABI ComputeCommandEncoderView
	{
		ComputeCommandEncoderView() = default;
#if RENDERER_VULKAN
		constexpr ComputeCommandEncoderView(VkCommandBuffer pCommandEncoder)
			: m_pCommandEncoder(pCommandEncoder)
		{
		}
		[[nodiscard]] operator VkCommandBuffer() const
		{
			return m_pCommandEncoder;
		}
#elif RENDERER_METAL
		constexpr ComputeCommandEncoderView(id<MTLComputeCommandEncoder> commandEncoder)
			: m_pCommandEncoder(commandEncoder)
		{
		}
		[[nodiscard]] operator id<MTLComputeCommandEncoder>() const
		{
			return m_pCommandEncoder;
		}
#elif WEBGPU_INDIRECT_HANDLES
		constexpr ComputeCommandEncoderView(WGPUComputePassEncoder* pCommandEncoder)
			: m_pCommandEncoder(pCommandEncoder)
		{
		}
		[[nodiscard]] operator WGPUComputePassEncoder() const
		{
			return m_pCommandEncoder != nullptr ? *m_pCommandEncoder : nullptr;
		}
#elif RENDERER_WEBGPU
		constexpr ComputeCommandEncoderView(WGPUComputePassEncoder pCommandEncoder)
			: m_pCommandEncoder(pCommandEncoder)
		{
		}
		[[nodiscard]] operator WGPUComputePassEncoder() const
		{
			return m_pCommandEncoder;
		}
#endif

		[[nodiscard]] bool IsValid() const
		{
#if RENDERER_VULKAN || RENDERER_WEBGPU || RENDERER_METAL
			return m_pCommandEncoder != nullptr;
#else
			return false;
#endif
		}

		void SetDebugName(const LogicalDevice& logicalDevice, const ConstZeroTerminatedStringView name) const;
		void BeginDebugMarker(const LogicalDevice& logicalDevice, const ConstZeroTerminatedStringView label, const Math::Color color) const;
		void EndDebugMarker(const LogicalDevice& logicalDevice) const;

		void BindDescriptorSets(
			const PipelineLayoutView pipelineLayout, const ArrayView<const DescriptorSetView, uint8> sets, const uint32 firstSetIndex = 0
		) const;
		void BindDynamicDescriptorSets(
			const PipelineLayoutView pipelineLayout,
			const ArrayView<const DescriptorSetView, uint8> sets,
			const ArrayView<const uint32, uint8> dynamicOffsets,
			const uint32 firstSetIndex = 0
		) const;
		void BindPipeline(const ComputePipeline& pipeline) const;

		void Dispatch(const Math::Vector3ui groupCount, const Math::Vector3ui groupSize) const;
	protected:
		friend struct CommandEncoder;

#if RENDERER_VULKAN
		VkCommandBuffer m_pCommandEncoder = nullptr;
#elif RENDERER_METAL
		id<MTLComputeCommandEncoder> m_pCommandEncoder;
#elif WEBGPU_INDIRECT_HANDLES
		WGPUComputePassEncoder* m_pCommandEncoder = nullptr;
#elif RENDERER_WEBGPU
		WGPUComputePassEncoder m_pCommandEncoder = nullptr;
#endif
	};

	struct ComputeDebugMarker
	{
		ComputeDebugMarker(
			const ComputeCommandEncoderView computeCommandEncoder,
			const LogicalDevice& logicalDevice,
			const ConstZeroTerminatedStringView label,
			const Math::Color color
		)
			: m_computeCommandEncoder(computeCommandEncoder)
			, m_logicalDevice(logicalDevice)
		{
			computeCommandEncoder.BeginDebugMarker(logicalDevice, label, color);
		}
		~ComputeDebugMarker()
		{
			m_computeCommandEncoder.EndDebugMarker(m_logicalDevice);
		}
	private:
		const ComputeCommandEncoderView m_computeCommandEncoder;
		const LogicalDevice& m_logicalDevice;
	};
}
