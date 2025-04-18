#pragma once

#include <Renderer/Vulkan/ForwardDeclares.h>
#include <Renderer/Metal/ForwardDeclares.h>
#include <Renderer/WebGPU/ForwardDeclares.h>

#include <Renderer/PipelineStageFlags.h>
#include <Common/Platform/ForceInline.h>
#include <Common/Platform/TrivialABI.h>

#include <Common/Memory/Containers/ArrayView.h>
#include <Common/Memory/Containers/ForwardDeclarations/ByteView.h>
#include <Common/EnumFlags.h>

struct VkSubmitInfo;
struct VkPresentInfoKHR;

namespace ngine::Rendering
{
	struct SwapchainView;
	struct FenceView;
	struct SemaphoreView;
	struct EncodedCommandBufferView;
	struct FrameImageId;
	struct Buffer;

	struct TRIVIAL_ABI CommandQueueView
	{
		CommandQueueView() = default;

#if RENDERER_VULKAN
		CommandQueueView(VkQueue pQueue)
			: m_pQueue(pQueue)
		{
		}

		[[nodiscard]] operator VkQueue() const
		{
			return m_pQueue;
		}
#elif RENDERER_METAL
		CommandQueueView(id<MTLCommandQueue> commandQueue)
			: m_pQueue(commandQueue)
		{
		}

		[[nodiscard]] operator id<MTLCommandQueue>() const
		{
			return m_pQueue;
		}
#elif RENDERER_WEBGPU
		CommandQueueView(WGPUQueue pQueue)
			: m_pQueue(pQueue)
		{
		}

		[[nodiscard]] operator WGPUQueue() const
		{
			return m_pQueue;
		}
#endif

		struct SubmitInfo
		{
			SubmitInfo(
				const ArrayView<const SemaphoreView, uint16> waitSemaphores,
				const ArrayView<const EnumFlags<PipelineStageFlags>, uint16> waitStageMasks,
				const ArrayView<const EncodedCommandBufferView, uint16> commandBuffers,
				const ArrayView<const SemaphoreView, uint16> signalSemaphores
			);

			[[nodiscard]] ArrayView<const SemaphoreView, uint16> GetWaitSemaphores() const
			{
				return {m_pWaitSemaphores, (uint16)m_waitSemaphoreCount};
			}
			[[nodiscard]] const ArrayView<const EnumFlags<PipelineStageFlags>, uint16> GetWaitStageMasks() const
			{
				return {m_pWaitStageMasks, (uint16)m_waitSemaphoreCount};
			}
			[[nodiscard]] const ArrayView<const EncodedCommandBufferView, uint16> GetEncodedCommandBuffers() const
			{
				return {m_pCommandBuffers, (uint16)m_commandBufferCount};
			}
			[[nodiscard]] const ArrayView<const SemaphoreView, uint16> GetSignalSemaphores() const
			{
				return {m_pSignalSemaphores, (uint16)m_signalSemaphoreCount};
			}
		protected:
#if RENDERER_VULKAN
			uint32 m_type{4};
			const void* m_pNext{nullptr};
#endif
			uint32 m_waitSemaphoreCount;
			const SemaphoreView* m_pWaitSemaphores;
			const EnumFlags<PipelineStageFlags>* m_pWaitStageMasks;
			uint32 m_commandBufferCount;
			const EncodedCommandBufferView* m_pCommandBuffers;
			uint32 m_signalSemaphoreCount;
			const SemaphoreView* m_pSignalSemaphores;
		};

		[[nodiscard]] bool
		Present(const ArrayView<const SemaphoreView, uint8> waitSemaphores, const SwapchainView swapchain, const FrameImageId imageIndex) const;
		bool WaitUntilIdle() const;

		[[nodiscard]] bool IsValid() const
		{
#if RENDERER_VULKAN || RENDERER_WEBGPU || RENDERER_METAL
			return m_pQueue != 0;
#else
			return false;
#endif
		}

		[[nodiscard]] bool operator==([[maybe_unused]] const CommandQueueView& other) const
		{
#if RENDERER_VULKAN || RENDERER_WEBGPU || RENDERER_METAL
			return m_pQueue == other.m_pQueue;
#else
			return false;
#endif
		}
		[[nodiscard]] bool operator!=([[maybe_unused]] const CommandQueueView& other) const
		{
			return !operator==(other);
		}
	protected:
#if RENDERER_VULKAN
		VkQueue m_pQueue = 0;
#elif RENDERER_METAL
		id<MTLCommandQueue> m_pQueue;
#elif RENDERER_WEBGPU
		WGPUQueue m_pQueue = nullptr;
#endif
	};
}
