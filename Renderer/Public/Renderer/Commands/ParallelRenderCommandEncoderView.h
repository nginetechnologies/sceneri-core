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

#include <Common/Math/Primitives/ForwardDeclarations/Rectangle.h>

namespace ngine::Rendering
{
	struct LogicalDevice;
	struct CommandBufferView;
	struct BufferView;
	struct PipelineLayoutView;
	struct GraphicsPipeline;

	struct TRIVIAL_ABI ParallelRenderCommandEncoderView
	{
		ParallelRenderCommandEncoderView() = default;
#if RENDERER_VULKAN
		constexpr ParallelRenderCommandEncoderView(VkCommandBuffer pCommandEncoder)
			: m_pCommandEncoder(pCommandEncoder)
		{
		}
		[[nodiscard]] operator VkCommandBuffer() const
		{
			return m_pCommandEncoder;
		}
#elif RENDERER_METAL
		constexpr ParallelRenderCommandEncoderView(id<MTLParallelRenderCommandEncoder> commandEncoder)
			: m_pCommandEncoder(commandEncoder)
		{
		}
		[[nodiscard]] operator id<MTLParallelRenderCommandEncoder>() const
		{
			return m_pCommandEncoder;
		}
#elif WEBGPU_INDIRECT_HANDLES
		constexpr ParallelRenderCommandEncoderView(WGPURenderBundleEncoder* pCommandEncoder)
			: m_pCommandEncoder(pCommandEncoder)
		{
		}
		[[nodiscard]] operator WGPURenderBundleEncoder() const
		{
			return m_pCommandEncoder != nullptr ? *m_pCommandEncoder : nullptr;
		}
#elif RENDERER_WEBGPU
		constexpr ParallelRenderCommandEncoderView(WGPURenderBundleEncoder pCommandEncoder)
			: m_pCommandEncoder(pCommandEncoder)
		{
		}
		[[nodiscard]] operator WGPURenderBundleEncoder() const
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

		void BindVertexBuffers(
			const ArrayView<const BufferView> buffers, const ArrayView<const uint64> offsets, const ArrayView<const uint64> size
		) const;
		void BindDescriptorSets(
			const PipelineLayoutView pipelineLayout, const ArrayView<const DescriptorSetView, uint8> sets, const uint32 firstSetIndex = 0
		) const;
		void BindDynamicDescriptorSets(
			const PipelineLayoutView pipelineLayout,
			const ArrayView<const DescriptorSetView, uint8> sets,
			const ArrayView<const uint32, uint8> dynamicOffsets,
			const uint32 firstSetIndex = 0
		) const;
		void BindPipeline(const GraphicsPipeline& pipeline) const;

		void SetDepthBias(const float depthBiasConstantFactor, const float depthBiasClamp, const float depthBiasSlopeFactor) const;

		void DrawIndexed(
			const BufferView buffer,
			const uint64 offset,
			const uint64 size,
			const uint32 indexCount,
			const uint32 instanceCount,
			const uint32 firstIndex = 0,
			const int32 vertexOffset = 0,
			const uint32 firstInstance = 0
		) const;
		void Draw(const uint32 vertexCount, const uint32 instanceCount, const uint32 firstVertex = 0, const uint32 firstInstance = 0) const;
		void DrawIndexedIndirect(const BufferView buffer, const uint64 offset, const uint32 drawCount = 1, const uint32 offsetStride = 0) const;
		void DrawIndirect(const BufferView buffer, const uint64 offset, const uint32 drawCount = 1, const uint32 offsetStride = 0) const;

		void StartNextSubpass() const;
	protected:
		friend struct CommandEncoder;

#if RENDERER_VULKAN
		VkCommandBuffer m_pCommandEncoder = nullptr;
#elif RENDERER_METAL
		id<MTLParallelRenderCommandEncoder> m_pCommandEncoder;
#elif WEBGPU_INDIRECT_HANDLES
		WGPURenderBundleEncoder* m_pCommandEncoder = nullptr;
#elif RENDERER_WEBGPU
		WGPURenderBundleEncoder m_pCommandEncoder = nullptr;
#endif
	};

	struct ParallelRenderDebugMarker
	{
		ParallelRenderDebugMarker(
			const ParallelRenderCommandEncoderView renderCommandEncoder,
			const LogicalDevice& logicalDevice,
			const ConstZeroTerminatedStringView label,
			const Math::Color color
		)
			: m_renderCommandEncoder(renderCommandEncoder)
			, m_logicalDevice(logicalDevice)
		{
			renderCommandEncoder.BeginDebugMarker(logicalDevice, label, color);
		}
		~ParallelRenderDebugMarker()
		{
			m_renderCommandEncoder.EndDebugMarker(m_logicalDevice);
		}
	private:
		const ParallelRenderCommandEncoderView m_renderCommandEncoder;
		const LogicalDevice& m_logicalDevice;
	};
}
