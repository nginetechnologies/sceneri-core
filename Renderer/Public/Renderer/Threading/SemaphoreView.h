#pragma once

#include <Renderer/Constants.h>
#include <Renderer/Vulkan/ForwardDeclares.h>
#include <Renderer/Metal/ForwardDeclares.h>
#include <Common/Platform/ForceInline.h>
#include <Common/Platform/TrivialABI.h>
#include <Common/Memory/ReferenceWrapper.h>

#include <Common/Memory/Containers/ForwardDeclarations/ZeroTerminatedStringView.h>

#if RENDERER_WEBGPU
#define EMULATED_SEMAPHORE_FROM_CALLBACK 1
#endif

namespace ngine::Rendering
{
	struct LogicalDeviceView;
	struct LogicalDevice;

#if EMULATED_SEMAPHORE_FROM_CALLBACK
	struct SemaphoreData;
	struct QueueSubmissionJob;
	struct EncodedCommandBufferView;
	struct CommandQueueSubmissionData;
#endif

	struct TRIVIAL_ABI SemaphoreView
	{
		SemaphoreView() = default;

#if RENDERER_VULKAN
		SemaphoreView(VkSemaphore pSemaphore)
			: m_pSemaphore(pSemaphore)
		{
		}

		[[nodiscard]] operator VkSemaphore() const
		{
			return m_pSemaphore;
		}
#elif RENDERER_METAL
		SemaphoreView(id<MTLEvent> event)
			: m_pSemaphore(event)
		{
		}

		[[nodiscard]] operator id<MTLEvent>() const
		{
			return m_pSemaphore;
		}
#elif EMULATED_SEMAPHORE_FROM_CALLBACK
		using Data = SemaphoreData;
		SemaphoreView(Data* pSemaphore)
			: m_pSemaphore(pSemaphore)
		{
		}
#endif

		[[nodiscard]] bool IsValid() const
		{
#if RENDERER_VULKAN || RENDERER_METAL || RENDERER_WEBGPU
			return m_pSemaphore != 0;
#else
			return false;
#endif
		}

		[[nodiscard]] bool operator==([[maybe_unused]] const SemaphoreView& other) const
		{
#if RENDERER_VULKAN || RENDERER_METAL || RENDERER_WEBGPU
			return m_pSemaphore == other.m_pSemaphore;
#else
			return false;
#endif
		}
		[[nodiscard]] bool operator!=(const SemaphoreView& other) const
		{
			return !operator==(other);
		}

		void SetDebugName(const LogicalDevice& logicalDevice, const ConstZeroTerminatedStringView name);
	protected:
#if EMULATED_SEMAPHORE_FROM_CALLBACK
		friend QueueSubmissionJob;
		friend CommandQueueSubmissionData;

		[[nodiscard]] bool EncodeWait(CommandQueueSubmissionData& waitingSubmissionData);
		void EncodeSignal();
		[[nodiscard]] ArrayView<const ReferenceWrapper<CommandQueueSubmissionData>> GetPendingSubmissionData();
		void Signal();
#endif
	protected:
#if RENDERER_VULKAN
		VkSemaphore m_pSemaphore = 0;
#elif RENDERER_METAL
		id<MTLEvent> m_pSemaphore{nullptr};
#elif EMULATED_SEMAPHORE_FROM_CALLBACK
		Data* m_pSemaphore = nullptr;
#endif
	};
}
