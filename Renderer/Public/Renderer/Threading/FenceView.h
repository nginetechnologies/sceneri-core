#pragma once

#include <Renderer/Vulkan/ForwardDeclares.h>
#include <Common/Platform/ForceInline.h>
#include <Common/Platform/TrivialABI.h>
#include <Common/Memory/Containers/ForwardDeclarations/ArrayView.h>

#if RENDERER_WEBGPU || RENDERER_METAL
#define EMULATED_FENCE_FROM_CALLBACK 1
#endif

namespace ngine::Rendering
{
	struct LogicalDeviceView;
	struct CommandQueueView;
	struct SwapchainOutput;
	struct VisionRenderOutput;

#if EMULATED_FENCE_FROM_CALLBACK
	struct FenceData;
	struct EncodedCommandBufferView;
	struct QueueSubmissionJob;
	struct CommandQueueSubmissionData;
#endif

	struct TRIVIAL_ABI FenceView
	{
		enum class Status : int
		{
			Signaled,
			Unsignaled,
			DeviceLost = -4
		};

		FenceView() = default;

#if RENDERER_VULKAN
		FenceView(VkFence pFence)
			: m_pFence(pFence)
		{
		}

		[[nodiscard]] operator VkFence() const
		{
			return m_pFence;
		}
#elif EMULATED_FENCE_FROM_CALLBACK
		using Data = FenceData;
		FenceView(Data* pFence)
			: m_pFence(pFence)
		{
		}
#endif

        enum class WaitResult : uint8
        {
            Error,
            Success,
            Timeout
        };

        WaitResult Wait(const LogicalDeviceView logicalDevice, const uint64 timeout) const;
		static WaitResult Wait(const LogicalDeviceView logicalDevice, const ArrayView<const FenceView, uint16> fences, const uint64 timeout);
		static void Reset(const LogicalDeviceView logicalDevice, const ArrayView<const FenceView, uint16> fences);
		void Reset(const LogicalDeviceView logicalDevice) const;
		[[nodiscard]] Status GetStatus(const LogicalDeviceView logicalDevice) const;

		[[nodiscard]] bool IsValid() const
		{
#if RENDERER_VULKAN || RENDERER_METAL || RENDERER_WEBGPU
			return m_pFence != 0;
#else
			return false;
#endif
		}
	protected:
		friend CommandQueueView;
		friend SwapchainOutput;
		friend VisionRenderOutput;
#if EMULATED_FENCE_FROM_CALLBACK
		friend QueueSubmissionJob;
		friend CommandQueueSubmissionData;

		void OnBeforeSubmit();
		void Signal();
#endif
	protected:
#if RENDERER_VULKAN
		VkFence m_pFence = 0;
#elif EMULATED_FENCE_FROM_CALLBACK
		Data* m_pFence = nullptr;
#endif
	};
}
