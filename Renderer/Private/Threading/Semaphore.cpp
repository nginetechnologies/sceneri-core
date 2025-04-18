#include "Threading/Semaphore.h"

#include "Devices/LogicalDevice.h"

#include <Common/Assert/Assert.h>
#include <Common/Math/Select.h>
#include <Common/Threading/AtomicInteger.h>

#include <Renderer/Vulkan/Includes.h>
#include <Renderer/Metal/Includes.h>

namespace ngine::Rendering
{
#if EMULATED_SEMAPHORE_FROM_CALLBACK
	struct SemaphoreData
	{
		Threading::Mutex m_mutex;
		InlineVector<ReferenceWrapper<CommandQueueSubmissionData>, 8> m_awaitingSubmissionData;
		Threading::Atomic<bool> m_wasSignaled{false};
	};
#endif

	Semaphore::Semaphore([[maybe_unused]] const LogicalDeviceView logicalDevice)
#if RENDERER_METAL
		: SemaphoreView([(id<MTLDevice>)logicalDevice newEvent])
#elif EMULATED_SEMAPHORE_FROM_CALLBACK
		: SemaphoreView(new Data{})
#endif
	{
#if RENDERER_VULKAN
		const VkSemaphoreCreateInfo semaphoreInfo = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, nullptr, 0};

		[[maybe_unused]] const VkResult creationResult = vkCreateSemaphore(logicalDevice, &semaphoreInfo, nullptr, &m_pSemaphore);
		Assert(creationResult == VK_SUCCESS);
#endif
	}

	Semaphore& Semaphore::operator=([[maybe_unused]] Semaphore&& other)
	{
		Assert(!IsValid(), "Destroy must have been called!");
#if RENDERER_VULKAN || RENDERER_METAL || RENDERER_WEBGPU
		m_pSemaphore = other.m_pSemaphore;
		other.m_pSemaphore = 0;
#endif
		return *this;
	}

	Semaphore::~Semaphore()
	{
		Assert(!IsValid(), "Destroy must have been called!");
	}

	void Semaphore::Destroy([[maybe_unused]] const LogicalDeviceView logicalDevice)
	{
#if RENDERER_VULKAN
		vkDestroySemaphore(logicalDevice, m_pSemaphore, nullptr);
		m_pSemaphore = 0;
#elif RENDERER_METAL
		m_pSemaphore = nil;
#elif EMULATED_SEMAPHORE_FROM_CALLBACK
		delete m_pSemaphore;
		m_pSemaphore = 0;
#endif
	}

#if RENDERER_OBJECT_DEBUG_NAMES
	void SemaphoreView::SetDebugName([[maybe_unused]] const LogicalDevice& logicalDevice, const ConstZeroTerminatedStringView name)
	{
#if RENDERER_VULKAN
		const VkDebugUtilsObjectNameInfoEXT debugInfo{
			VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
			nullptr,
			VK_OBJECT_TYPE_SEMAPHORE,
			reinterpret_cast<uint64_t>(m_pSemaphore),
			name
		};

#if PLATFORM_APPLE
		vkSetDebugUtilsObjectNameEXT(logicalDevice, &debugInfo);
#else
		const PFN_vkSetDebugUtilsObjectNameEXT setDebugUtilsObjectNameEXT =
			reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(logicalDevice.GetSetDebugUtilsObjectNameEXT());
		if (setDebugUtilsObjectNameEXT != nullptr)
		{
			setDebugUtilsObjectNameEXT(logicalDevice, &debugInfo);
		}
#endif

#elif RENDERER_METAL
		[m_pSemaphore setLabel:[NSString stringWithUTF8String:name]];
#elif RENDERER_WEBGPU
		UNUSED(name);
#endif
	}
#endif

#if EMULATED_SEMAPHORE_FROM_CALLBACK
	bool SemaphoreView::EncodeWait(CommandQueueSubmissionData& waitingSubmissionData)
	{
		SemaphoreData* pData = m_pSemaphore;
		Threading::UniqueLock lock(pData->m_mutex);
		if (pData->m_wasSignaled)
		{
			// Indicate that this semaphore was already signaled, skip waiting for it
			return false;
		}
		else
		{
			pData->m_awaitingSubmissionData.EmplaceBack(waitingSubmissionData);
			return true;
		}
	}

	void SemaphoreView::EncodeSignal()
	{
		SemaphoreData* pData = m_pSemaphore;
		Threading::UniqueLock lock(pData->m_mutex);
		Assert(pData->m_awaitingSubmissionData.IsEmpty());
		pData->m_wasSignaled = false;
	}

	ArrayView<const ReferenceWrapper<CommandQueueSubmissionData>> SemaphoreView::GetPendingSubmissionData()
	{
		SemaphoreData* pData = m_pSemaphore;
		Threading::UniqueLock lock(pData->m_mutex);
		return pData->m_awaitingSubmissionData.GetView();
	}

	void SemaphoreView::Signal()
	{
		SemaphoreData* pData = m_pSemaphore;
		Threading::UniqueLock lock(pData->m_mutex);
		pData->m_awaitingSubmissionData.Clear();

		bool expected = false;
		[[maybe_unused]] const bool wasSignaled = pData->m_wasSignaled.CompareExchangeStrong(expected, true);
		Assert(wasSignaled);
	}
#endif
}
