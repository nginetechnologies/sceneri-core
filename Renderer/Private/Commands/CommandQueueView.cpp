
#include <Renderer/RenderOutput/SwapchainView.h>
#include <Renderer/Commands/CommandQueueView.h>
#include "Threading/FenceView.h"
#include "Threading/SemaphoreView.h"
#include <Renderer/Commands/EncodedCommandBufferView.h>
#include "FrameImageId.h"

#include <Renderer/Buffers/Buffer.h>

#include <Renderer/Vulkan/Includes.h>
#include <Renderer/Metal/Includes.h>
#include <Renderer/WebGPU/Includes.h>

#include <Common/Threading/AtomicBool.h>

#if RENDERER_WEBGPU
#include <Common/Threading/Jobs/JobRunnerThread.h>
#include <Renderer/Window/Window.h>
#endif

namespace ngine::Rendering
{
	CommandQueueView::SubmitInfo::SubmitInfo(
		const ArrayView<const SemaphoreView, uint16> waitSemaphores,
		const ArrayView<const EnumFlags<PipelineStageFlags>, uint16> waitStageMasks,
		const ArrayView<const EncodedCommandBufferView, uint16> commandBuffers,
		const ArrayView<const SemaphoreView, uint16> signalSemaphores
	)
		: m_waitSemaphoreCount(waitSemaphores.GetSize())
		, m_pWaitSemaphores(waitSemaphores.GetData())
		, m_pWaitStageMasks(waitStageMasks.GetData())
		, m_commandBufferCount(commandBuffers.GetSize())
		, m_pCommandBuffers(commandBuffers.GetData())
		, m_signalSemaphoreCount(signalSemaphores.GetSize())
		, m_pSignalSemaphores(signalSemaphores.GetData())
	{
		// TODO: MultiArrayView, views into multiple arrays but one size
		Assert(waitSemaphores.GetSize() == waitStageMasks.GetSize());
	}

	bool CommandQueueView::Present(
		[[maybe_unused]] const ArrayView<const SemaphoreView, uint8> waitSemaphores,
		[[maybe_unused]] const SwapchainView swapchain,
		[[maybe_unused]] const FrameImageId imageIndex
	) const
	{
#if RENDERER_VULKAN
		const uint32 sentImageIndex = (uint8)imageIndex;

		VkSwapchainKHR pSwapchain = swapchain;
		const VkPresentInfoKHR presentInfo = {
			VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			nullptr,
			waitSemaphores.GetSize(),
			reinterpret_cast<const VkSemaphore*>(waitSemaphores.GetData()),
			1,
			&pSwapchain,
			&sentImageIndex,
			nullptr
		};

		[[maybe_unused]] const VkResult result = vkQueuePresentKHR(m_pQueue, &presentInfo);
		return result == VK_SUCCESS;
#elif RENDERER_WEBGPU
		Assert(waitSemaphores.IsEmpty(), "Wait semaphores not supported!");

#if !PLATFORM_WEB
		wgpuSurfacePresent(swapchain);
#endif
		return true;
#else
		return false;
#endif
	}

	bool CommandQueueView::WaitUntilIdle() const
	{
#if RENDERER_VULKAN
		return vkQueueWaitIdle(m_pQueue) == VK_SUCCESS;
#elif RENDERER_METAL
		id<MTLCommandBuffer> commandBuffer = [m_pQueue commandBuffer];
		[commandBuffer commit];
		[commandBuffer waitUntilCompleted];
		return true;
#elif RENDERER_WEBGPU
		Threading::Atomic<bool> completed{false};
		Rendering::Window::QueueOnWindowThread(
			[pQueue = m_pQueue, &completed]()
			{
				wgpuQueueOnSubmittedWorkDone(
					pQueue,
#if RENDERER_WEBGPU_DAWN2
					WGPUQueueWorkDoneCallbackInfo
					{
						nullptr,
						WGPUCallbackMode_AllowSpontaneous,
						[]([[maybe_unused]] const WGPUQueueWorkDoneStatus status, void* pUserData1, [[maybe_unused]] void* pUserData2)
						{
							Threading::Atomic<bool>& completed = *reinterpret_cast<Threading::Atomic<bool>*>(pUserData1);
							completed = true;
						},
						&completed
					}
#else
					[]([[maybe_unused]] const WGPUQueueWorkDoneStatus status, void* pUserData)
					{
						Threading::Atomic<bool>& completed = *reinterpret_cast<Threading::Atomic<bool>*>(pUserData);
						completed = true;
					},
					&completed
#endif
				);
			}
		);

		Threading::JobRunnerThread& thread = *Threading::JobRunnerThread::GetCurrent();
		while (!completed)
		{
			thread.DoRunNextJob();
		}

		return true;
#else
		Assert(false, "TODO");
		return false;
#endif
	}
}
