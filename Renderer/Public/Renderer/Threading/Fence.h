#pragma once

#include "FenceView.h"

namespace ngine::Rendering
{
	struct LogicalDeviceView;

	struct Fence : public FenceView
	{
		Fence() = default;
		Fence(const LogicalDeviceView logicalDevice, const Status defaultState = Status::Signaled);
		Fence(const Fence&) = delete;
		Fence& operator=(const Fence&) = delete;
		Fence([[maybe_unused]] Fence&& other)
#if RENDERER_VULKAN || RENDERER_METAL || RENDERER_WEBGPU
			: FenceView(other.m_pFence)
#endif
		{
#if RENDERER_VULKAN || RENDERER_METAL || RENDERER_WEBGPU
			other.m_pFence = 0;
#endif
		}
		Fence& operator=(Fence&& other);
		~Fence();

		void Destroy(const LogicalDeviceView logicalDevice);
	};
}
