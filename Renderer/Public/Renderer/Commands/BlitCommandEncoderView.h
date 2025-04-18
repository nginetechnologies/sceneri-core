#pragma once

#include <Common/Memory/Containers/ForwardDeclarations/ArrayView.h>
#include <Common/Platform/ForceInline.h>
#include <Common/Platform/TrivialABI.h>

#include <Renderer/Constants.h>
#include <Renderer/Devices/QueueFamily.h>
#include <Renderer/Vulkan/ForwardDeclares.h>
#include <Renderer/Metal/ForwardDeclares.h>
#include <Renderer/WebGPU/ForwardDeclares.h>

#if PROFILE_BUILD
#include <Common/Memory/Containers/ZeroTerminatedStringView.h>
#include <Common/Math/Color.h>
#endif

#include <Common/Math/ForwardDeclarations/Vector3.h>

namespace ngine::Rendering
{
	struct LogicalDevice;
	struct CommandBufferView;
	struct BufferView;
	struct StagingBuffer;
	struct DataToBufferBatch;
	struct RenderTexture;

	struct TRIVIAL_ABI BlitCommandEncoderView
	{
		BlitCommandEncoderView() = default;
#if RENDERER_VULKAN
		constexpr BlitCommandEncoderView(VkCommandBuffer pCommandEncoder)
			: m_pCommandEncoder(pCommandEncoder)
		{
		}
		[[nodiscard]] operator VkCommandBuffer() const
		{
			return m_pCommandEncoder;
		}
#elif RENDERER_METAL
		constexpr BlitCommandEncoderView(id<MTLBlitCommandEncoder> commandEncoder)
			: m_pCommandEncoder(commandEncoder)
		{
		}
		[[nodiscard]] operator id<MTLBlitCommandEncoder>() const
		{
			return m_pCommandEncoder;
		}
#elif WEBGPU_INDIRECT_HANDLES
		constexpr BlitCommandEncoderView(WGPUCommandEncoder* pCommandEncoder)
			: m_pCommandEncoder(pCommandEncoder)
		{
		}
		[[nodiscard]] operator WGPUCommandEncoder() const
		{
			return m_pCommandEncoder != nullptr ? *m_pCommandEncoder : nullptr;
		}
#elif RENDERER_WEBGPU
		constexpr BlitCommandEncoderView(WGPUCommandEncoder pCommandEncoder)
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
#if RENDERER_VULKAN || RENDERER_WEBGPU || RENDERER_METAL
			return m_pCommandEncoder != nullptr;
#else
			return false;
#endif
		}

		void SetDebugName(const LogicalDevice& logicalDevice, const ConstZeroTerminatedStringView name) const;
		void BeginDebugMarker(const LogicalDevice& logicalDevice, const ConstZeroTerminatedStringView label, const Math::Color color) const;
		void EndDebugMarker(const LogicalDevice& logicalDevice) const;

		void RecordCopyBufferToBuffer(
			const BufferView sourceBuffer, const BufferView targetBuffer, const ArrayView<const BufferCopy, uint16> regions
		) const;
		void RecordCopyDataToBuffer(
			LogicalDevice& logicalDevice,
			const QueueFamily queueFamily,
			const ArrayView<const DataToBufferBatch, uint16> batches,
			Optional<StagingBuffer>& stagingBufferOut
		) const;
		void RecordCopyBufferToImage(
			const BufferView sourceBuffer,
			const RenderTexture& targetImage,
			const ImageLayout imageLayout,
			const ArrayView<const BufferImageCopy, uint16> regions
		) const;
		void RecordCopyImageToBuffer(
			const RenderTexture& sourceImage,
			const ImageLayout sourceImageLayout,
			const BufferView targetBuffer,
			const ArrayView<const BufferImageCopy, uint16> regions
		) const;
		void RecordCopyImageToImage(
			const RenderTexture& sourceImage,
			const ImageLayout sourceImageLayout,
			const RenderTexture& targetImage,
			const ImageLayout targetImageLayout,
			const ArrayView<const ImageCopy, uint16> regions
		) const;
		void RecordBlitImage(
			const RenderTexture& targetImage,
			const ImageLayout targetImageLayout,
			const RenderTexture& sourceImage,
			const ImageLayout sourceImageLayout,
			const ArrayView<const ImageBlit, uint16> region
		) const;
	protected:
		friend struct CommandEncoder;

#if RENDERER_VULKAN
		VkCommandBuffer m_pCommandEncoder = nullptr;
#elif RENDERER_METAL
		id<MTLBlitCommandEncoder> m_pCommandEncoder;
#elif WEBGPU_INDIRECT_HANDLES
		WGPUCommandEncoder* m_pCommandEncoder = nullptr;
#elif RENDERER_WEBGPU
		WGPUCommandEncoder m_pCommandEncoder = nullptr;
#endif
	};

	struct BlitDebugMarker
	{
		BlitDebugMarker(
			const BlitCommandEncoderView blitCommandEncoder,
			const LogicalDevice& logicalDevice,
			const ConstZeroTerminatedStringView label,
			const Math::Color color
		)
			: m_blitCommandEncoder(blitCommandEncoder)
			, m_logicalDevice(logicalDevice)
		{
			blitCommandEncoder.BeginDebugMarker(logicalDevice, label, color);
		}
		~BlitDebugMarker()
		{
			m_blitCommandEncoder.EndDebugMarker(m_logicalDevice);
		}
	private:
		const BlitCommandEncoderView m_blitCommandEncoder;
		const LogicalDevice& m_logicalDevice;
	};
}
