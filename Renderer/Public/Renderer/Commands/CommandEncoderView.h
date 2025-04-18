#pragma once

#include <Common/Memory/Containers/ForwardDeclarations/ArrayView.h>
#include <Common/Memory/Align.h>
#include <Common/Math/ForwardDeclarations/Vector3.h>
#include <Common/Math/Primitives/ForwardDeclarations/Rectangle.h>
#include <Common/Platform/ForceInline.h>
#include <Common/Platform/TrivialABI.h>
#include <Common/EnumFlags.h>

#include <Common/Math/Vector2.h>
#include <Common/Math/Vector3.h>

#include <Renderer/Constants.h>
#include <Renderer/ImageLayout.h>
#include <Renderer/PipelineStageFlags.h>
#include <Renderer/AccessFlags.h>
#include <Renderer/ImageAspectFlags.h>
#include <Renderer/Assets/Texture/ArrayRange.h>

#include <Renderer/Vulkan/ForwardDeclares.h>
#include <Renderer/Metal/ForwardDeclares.h>
#include <Renderer/WebGPU/ForwardDeclares.h>

#include <Common/Memory/Containers/ZeroTerminatedStringView.h>
#include <Common/Math/Color.h>

namespace ngine::Rendering
{
	struct BufferView;
	struct DescriptorSetView;
	struct ImageView;
	struct ImageMemoryBarrier;
	struct BufferMemoryBarrier;
	struct LogicalDevice;
	struct LogicalDeviceView;
	struct CommandBufferView;

	struct SubresourceLayers
	{
		EnumFlags<ImageAspectFlags> aspectMask;
		uint32 mipLevel = 0u;
		ArrayRange arrayLayerRanges = {0u, 1u};
	};

	struct BufferCopy
	{
		BufferCopy(const uint64 sourceOffset, const uint64 destinationOffset, const uint64 size)
			: m_sourceOffset(sourceOffset)
			, m_destinationOffset(destinationOffset)
			, m_size(size)
		{
		}

		uint64 m_sourceOffset = 0;
		uint64 m_destinationOffset = 0;
		uint64 m_size = 0;
	};

#if RENDERER_VULKAN
	namespace Internal
	{
		using Offset3 = Math::UnalignedVector3<int>;
		using Extent3 = Math::UnalignedVector3<uint32>;
	}
#endif

	struct ImageCopy
	{
		ImageCopy(
			const SubresourceLayers sourceLayers,
			const Math::Vector3i sourceOffset,
			const SubresourceLayers targetLayers,
			const Math::Vector3i targetOffset,
			const Math::Vector3ui extent
		)
			: m_sourceLayers(sourceLayers)
			, m_sourceOffset(sourceOffset)
			, m_targetLayers(targetLayers)
			, m_targetOffset(targetOffset)
			, m_extent(extent)
		{
		}

		SubresourceLayers m_sourceLayers;
#if RENDERER_VULKAN
		Internal::Offset3 m_sourceOffset;
#else
		Math::Vector3i m_sourceOffset;
#endif
		SubresourceLayers m_targetLayers;
#if RENDERER_VULKAN
		Internal::Offset3 m_targetOffset;
		Internal::Extent3 m_extent;
#else
		Math::Vector3i m_targetOffset;
		Math::Vector3ui m_extent;
#endif
	};

	struct BufferImageCopy
	{
		BufferImageCopy(
			const uint64 bufferOffset,
			[[maybe_unused]] const Math::Vector2ui bufferBytesPerDimension,
			[[maybe_unused]] const Math::Vector2ui blockCountPerDimension,
			[[maybe_unused]] const Math::TVector3<uint8> blockExtent,
			const SubresourceLayers imageSubresource,
			const Math::Vector3i imageOffset,
			const Math::Vector3ui imageExtent
		)
			: m_bufferOffset(bufferOffset)
#if RENDERER_VULKAN
			, m_bufferBytesPerDimension{Memory::Align(imageExtent.x, blockExtent.x), Memory::Align(imageExtent.y, blockExtent.y)}
#else
			, m_bufferBytesPerDimension(bufferBytesPerDimension)
#endif
#if RENDERER_WEBGPU
			, m_blockCountPerDimension(blockCountPerDimension)
#endif
			, m_imageSubresource(imageSubresource)
			, m_imageOffset(imageOffset)
			, m_imageExtent(imageExtent)
		{
		}

		uint64 m_bufferOffset;
		Math::Vector2ui m_bufferBytesPerDimension;
#if RENDERER_WEBGPU
		Math::Vector2ui m_blockCountPerDimension;
#endif

		SubresourceLayers m_imageSubresource;
#if RENDERER_VULKAN
		Internal::Offset3 m_imageOffset;
		Internal::Extent3 m_imageExtent;
#else
		Math::Vector3i m_imageOffset;
		Math::Vector3ui m_imageExtent;
#endif
	};

	struct ImageBlit
	{
		ImageBlit(
			const SubresourceLayers sourceLayers,
			const Math::Vector3i sourceOffset,
			const Math::Vector3ui sourceExtent,
			const SubresourceLayers targetLayers,
			const Math::Vector3i targetOffset,
			const Math::Vector3ui targetExtent
		)
			: m_sourceLayers(sourceLayers)
			, m_sourceOffset{sourceOffset}
			, m_sourceExtent{(Math::Vector3i)sourceExtent}
			, m_targetLayers(targetLayers)
			, m_targetOffset{targetOffset}
			, m_targetExtent{(Math::Vector3i)targetExtent}
		{
		}

		SubresourceLayers m_sourceLayers;
#if RENDERER_VULKAN
		Internal::Offset3 m_sourceOffset;
		Internal::Offset3 m_sourceExtent;
#else
		Math::Vector3i m_sourceOffset;
		Math::Vector3i m_sourceExtent;
#endif
		SubresourceLayers m_targetLayers;
#if RENDERER_VULKAN
		Internal::Offset3 m_targetOffset;
		Internal::Offset3 m_targetExtent;
#else
		Math::Vector3i m_targetOffset;
		Math::Vector3i m_targetExtent;
#endif
	};

	struct RenderCommandEncoder;
	struct ParallelRenderCommandEncoder;
	struct ComputeCommandEncoder;
	struct BlitCommandEncoder;
	struct BarrierCommandEncoder;
	struct AccelerationStructureCommandEncoder;

	struct RenderPassView;
	struct FramebufferView;
	struct ClearValue;

	struct TRIVIAL_ABI CommandEncoderView
	{
		CommandEncoderView() = default;
#if RENDERER_VULKAN
		constexpr CommandEncoderView(VkCommandBuffer pCommandEncoder)
			: m_pCommandEncoder(pCommandEncoder)
		{
		}

		[[nodiscard]] operator VkCommandBuffer() const
		{
			return m_pCommandEncoder;
		}
#elif RENDERER_METAL
		constexpr CommandEncoderView(id<MTLCommandBuffer> pCommandEncoder)
			: m_pCommandEncoder(pCommandEncoder)
		{
		}

		[[nodiscard]] operator id<MTLCommandBuffer>() const
		{
			return m_pCommandEncoder;
		}
#elif WEBGPU_INDIRECT_HANDLES
		constexpr CommandEncoderView(WGPUCommandEncoder* pCommandEncoder)
			: m_pCommandEncoder(pCommandEncoder)
		{
		}

		[[nodiscard]] operator WGPUCommandEncoder() const
		{
			return m_pCommandEncoder != nullptr ? *m_pCommandEncoder : nullptr;
		}
#elif RENDERER_WEBGPU
		constexpr CommandEncoderView(WGPUCommandEncoder pCommandEncoder)
			: m_pCommandEncoder(pCommandEncoder)
		{
		}

		[[nodiscard]] operator WGPUCommandEncoder() const
		{
			return m_pCommandEncoder;
		}
#endif

		[[nodiscard]] bool IsValid() const
		{
#if RENDERER_VULKAN || RENDERER_METAL || RENDERER_WEBGPU
			return m_pCommandEncoder != nullptr;
#else
			return false;
#endif
		}

		void SetDebugName(const LogicalDevice& logicalDevice, const ConstZeroTerminatedStringView name) const;
		void BeginDebugMarker(const LogicalDevice& logicalDevice, const ConstZeroTerminatedStringView label, const Math::Color color) const;
		void EndDebugMarker(const LogicalDevice& logicalDevice) const;

		void RecordPipelineBarrier(
			const EnumFlags<PipelineStageFlags> sourceStage,
			const EnumFlags<PipelineStageFlags> targetStage,
			const ArrayView<const ImageMemoryBarrier> imageBarriers,
			const ArrayView<const BufferMemoryBarrier> bufferBarriers
		) const;

		[[nodiscard]] RenderCommandEncoder BeginRenderPass(
			LogicalDevice& logicalDevice,
			const RenderPassView renderPass,
			const FramebufferView framebuffer,
			const Math::Rectangleui extent,
			const ArrayView<const ClearValue, uint8> clearValues,
			const uint32 maximumPushConstantInstanceCount
		) const;
		[[nodiscard]] ParallelRenderCommandEncoder BeginParallelRenderPass(
			LogicalDevice& logicalDevice,
			const RenderPassView renderPass,
			const FramebufferView framebuffer,
			const Math::Rectangleui extent,
			const ArrayView<const ClearValue, uint8> clearValues,
			const uint32 maximumPushConstantInstanceCount
		) const;

		[[nodiscard]] ComputeCommandEncoder BeginCompute(LogicalDevice& logicalDevice, const uint32 maximumPushConstantInstanceCount) const;
		[[nodiscard]] BlitCommandEncoder BeginBlit() const;
		[[nodiscard]] BarrierCommandEncoder BeginBarrier() const;

		[[nodiscard]] AccelerationStructureCommandEncoder BeginAccelerationStructure() const;
	protected:
		friend struct CommandEncoder;

#if RENDERER_VULKAN
		VkCommandBuffer m_pCommandEncoder = nullptr;
#elif RENDERER_METAL
		id<MTLCommandBuffer> m_pCommandEncoder;
#elif WEBGPU_INDIRECT_HANDLES
		WGPUCommandEncoder* m_pCommandEncoder = nullptr;
#elif RENDERER_WEBGPU
		WGPUCommandEncoder m_pCommandEncoder = nullptr;
#endif
	};

	struct DebugMarker
	{
		DebugMarker(
			const CommandEncoderView commandEncoder,
			const LogicalDevice& logicalDevice,
			const ConstZeroTerminatedStringView label,
			const Math::Color color
		)
			: m_commandEncoder(commandEncoder)
			, m_logicalDevice(logicalDevice)
		{
			commandEncoder.BeginDebugMarker(logicalDevice, label, color);
		}
		~DebugMarker()
		{
			m_commandEncoder.EndDebugMarker(m_logicalDevice);
		}
	private:
		const CommandEncoderView m_commandEncoder;
		const LogicalDevice& m_logicalDevice;
	};
}
