#pragma once

#include "SemaphoreView.h"

#if RENDERER_OBJECT_DEBUG_NAMES
#include <Common/Memory/Containers/String.h>
#endif

namespace ngine::Rendering
{
	struct LogicalDeviceView;

	struct Semaphore : public SemaphoreView
	{
		Semaphore() = default;
		Semaphore(const LogicalDeviceView logicalDevice);
		Semaphore(const Semaphore&) = delete;
		Semaphore& operator=(const Semaphore&) = delete;
		Semaphore([[maybe_unused]] Semaphore&& other)
#if RENDERER_VULKAN || RENDERER_METAL || RENDERER_WEBGPU
			: SemaphoreView(other.m_pSemaphore)
#endif
		{
#if RENDERER_VULKAN || RENDERER_METAL || RENDERER_WEBGPU
			other.m_pSemaphore = 0;
#endif
		}
		Semaphore& operator=(Semaphore&& other);
		~Semaphore();

		void Destroy(const LogicalDeviceView logicalDevice);

#if RENDERER_OBJECT_DEBUG_NAMES
		void SetDebugName(const LogicalDevice& logicalDevice, String&& name)
		{
			m_debugName = Forward<String>(name);
			SemaphoreView::SetDebugName(logicalDevice, m_debugName);
		}
#endif
	protected:
#if RENDERER_OBJECT_DEBUG_NAMES
		String m_debugName;
#endif
	};
}
