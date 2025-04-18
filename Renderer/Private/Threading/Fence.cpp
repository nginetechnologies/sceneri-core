#include "Threading/Fence.h"

#include "Devices/LogicalDeviceView.h"

#include <Common/Assert/Assert.h>
#include <Common/Math/Select.h>
#include <Common/Memory/Containers/ArrayView.h>
#include <Common/Memory/Containers/InlineVector.h>
#include <Common/Platform/Unused.h>

#include <Renderer/Vulkan/Includes.h>

#if EMULATED_FENCE_FROM_CALLBACK
#include <Common/Threading/AtomicEnum.h>
#include <Common/Threading/AtomicInteger.h>
#include <Common/Threading/Mutexes/Mutex.h>
#include <Common/Threading/Mutexes/ConditionVariable.h>
#include <Common/Memory/Containers/Vector.h>

#include <Renderer/Commands/EncodedCommandBufferView.h>
#endif

namespace ngine::Rendering
{
#if EMULATED_FENCE_FROM_CALLBACK
	struct FenceData
	{
		Threading::Atomic<FenceView::Status> m_status{FenceView::Status::Unsignaled};
		Threading::Mutex m_mutex;
		Threading::ConditionVariable m_conditionVariable;
	};
#endif

	Fence::Fence([[maybe_unused]] const LogicalDeviceView logicalDevice, const Status defaultState)
	{
#if RENDERER_VULKAN
		const VkFenceCreateInfo fenceInfo = {
			VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
			nullptr,
			Math::Select(defaultState == Status::Signaled, VkFenceCreateFlags(VK_FENCE_CREATE_SIGNALED_BIT), VkFenceCreateFlags())
		};

		[[maybe_unused]] const VkResult result = vkCreateFence(logicalDevice, &fenceInfo, nullptr, &m_pFence);
		Assert(result == VK_SUCCESS);
#elif EMULATED_FENCE_FROM_CALLBACK
		m_pFence = new Data{defaultState};
#else
		UNUSED(logicalDevice);
		UNUSED(defaultState);
#endif
	}

	Fence& Fence::operator=([[maybe_unused]] Fence&& other)
	{
		Assert(!IsValid(), "Destroy must have been called!");
#if RENDERER_VULKAN || RENDERER_WEBGPU || RENDERER_METAL
		m_pFence = other.m_pFence;
		other.m_pFence = 0;
#endif
		return *this;
	}

	Fence::~Fence()
	{
		Assert(!IsValid(), "Destroy must have been called!");
	}

	void Fence::Destroy([[maybe_unused]] const LogicalDeviceView logicalDevice)
	{
		Assert(IsValid());

#if RENDERER_VULKAN
		vkDestroyFence(logicalDevice, m_pFence, nullptr);
		m_pFence = 0;
#elif EMULATED_FENCE_FROM_CALLBACK
		delete m_pFence;
		m_pFence = nullptr;
#endif
	}

	FenceView::WaitResult FenceView::Wait([[maybe_unused]] const LogicalDeviceView logicalDevice, [[maybe_unused]] const uint64 timeout) const
	{
		Assert(IsValid());

#if RENDERER_VULKAN
		switch (vkWaitForFences(logicalDevice, 1, &m_pFence, VK_TRUE, timeout))
		{
			case VK_SUCCESS:
				return WaitResult::Success;
			case VK_TIMEOUT:
				return WaitResult::Timeout;
			default:
				return WaitResult::Error;
		}
#elif EMULATED_FENCE_FROM_CALLBACK
		Data& fenceData = *m_pFence;
		if (fenceData.m_status == Status::Unsignaled)
		{
			Threading::UniqueLock lock(fenceData.m_mutex);
			while (fenceData.m_status == Status::Unsignaled)
			{
				fenceData.m_conditionVariable.Wait(lock);
			}
		}
		return WaitResult::Success;
#else
		return WaitResult::Error;
#endif
	}

	/* static */ FenceView::WaitResult FenceView::Wait(
		[[maybe_unused]] const LogicalDeviceView logicalDevice,
		[[maybe_unused]] const ArrayView<const FenceView, uint16> fences,
		[[maybe_unused]] const uint64 timeout
	)
	{
		Assert(fences.All(
			[](const FenceView& fence)
			{
				return fence.IsValid();
			}
		));

#if RENDERER_VULKAN
		const VkResult result =
			vkWaitForFences(logicalDevice, fences.GetSize(), reinterpret_cast<const VkFence*>(fences.GetData()), VK_TRUE, timeout);
		switch (result)
		{
			case VK_SUCCESS:
				return WaitResult::Success;
			case VK_TIMEOUT:
				return WaitResult::Timeout;
			default:
				return WaitResult::Error;
		}
#elif EMULATED_FENCE_FROM_CALLBACK
		if (!fences.All(
					[](const FenceView fence)
					{
						return fence.m_pFence->m_status == Status::Signaled;
					}
				))
		{
			InlineVector<FenceView, 8> lockedFences(Memory::Reserve, fences.GetSize());
			for (FenceView fence : fences)
			{
				if (fence.m_pFence->m_mutex.LockExclusive())
				{
					if (fence.m_pFence->m_status == Status::Unsignaled)
					{
						lockedFences.EmplaceBack(fence);
					}
					else
					{
						fence.m_pFence->m_mutex.UnlockExclusive();
					}
				}
				else
				{
					Assert(false, "Failed to lock!");
					return WaitResult::Error;
				}
			}

			for (const FenceView fence : lockedFences)
			{
				Threading::UniqueLock lock(Threading::AdoptLock, fence.m_pFence->m_mutex);
				while (fence.m_pFence->m_status == FenceView::Status::Unsignaled)
				{
					fence.m_pFence->m_conditionVariable.Wait(lock);
				}
			}
		}
		return WaitResult::Success;
#else
		return WaitResult::Error;
#endif
	}

	/* static */ void
	FenceView::Reset([[maybe_unused]] const LogicalDeviceView logicalDevice, [[maybe_unused]] const ArrayView<const FenceView, uint16> fences)
	{
		Assert(fences.All(
			[](const FenceView& fence)
			{
				return fence.IsValid();
			}
		));

#if RENDERER_VULKAN
		[[maybe_unused]] const VkResult result =
			vkResetFences(logicalDevice, fences.GetSize(), reinterpret_cast<const VkFence*>(fences.GetData()));
		Assert(result == VK_SUCCESS);
#elif EMULATED_FENCE_FROM_CALLBACK
		for (const FenceView fence : fences)
		{
			fence.m_pFence->m_status = Status::Unsignaled;
		}
#endif
	}

	void FenceView::Reset([[maybe_unused]] const LogicalDeviceView logicalDevice) const
	{
		Assert(IsValid());

#if RENDERER_VULKAN
		[[maybe_unused]] const VkResult result = vkResetFences(logicalDevice, 1, &m_pFence);
		Assert(result == VK_SUCCESS);
#elif EMULATED_FENCE_FROM_CALLBACK
		m_pFence->m_status = Status::Unsignaled;
#endif
	}

	FenceView::Status FenceView::GetStatus([[maybe_unused]] const LogicalDeviceView logicalDevice) const
	{
		Assert(IsValid());

#if RENDERER_VULKAN
		return static_cast<Status>(vkGetFenceStatus(logicalDevice, m_pFence));
#elif EMULATED_FENCE_FROM_CALLBACK
		return m_pFence->m_status;
#else
		return Status::DeviceLost;
#endif
	}

#if EMULATED_FENCE_FROM_CALLBACK
	void FenceView::OnBeforeSubmit()
	{
	}

	void FenceView::Signal()
	{
		Assert(IsValid());
		Data& fenceData = *m_pFence;
		{
			Threading::UniqueLock lock(fenceData.m_mutex);
			Assert(fenceData.m_status == Status::Unsignaled);
			fenceData.m_status = Status::Signaled;
		}
		fenceData.m_conditionVariable.NotifyAll();
	}
#endif
}
