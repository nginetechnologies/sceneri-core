#pragma once

#include "CommandPoolView.h"

#include <Renderer/Devices/QueueFamily.h>
#include <Common/EnumFlags.h>

namespace ngine::Rendering
{
	struct PhysicalDevice;
	struct LogicalDeviceView;
	struct CommandBuffer;

	struct CommandPool : public CommandPoolView
	{
		enum class Flags : uint8
		{
			/* maps to VkCommandPoolCreateFlagBits */
			SupportIndividualCommandBufferReset = 1 << 1
		};

		CommandPool() = default;
		CommandPool(const LogicalDeviceView logicalDevice, const uint32 queueFamilyIndex, const EnumFlags<Flags> flags = EnumFlags<Flags>());
		CommandPool(const CommandPool&) = delete;
		CommandPool& operator=(const CommandPool&) = delete;
		CommandPool([[maybe_unused]] CommandPool&& other) noexcept
		{
#if RENDERER_VULKAN
			m_pCommandPool = other.m_pCommandPool;
			other.m_pCommandPool = 0;
#endif
		}
		CommandPool& operator=(CommandPool&& other) noexcept;
		~CommandPool();

		void Destroy(const LogicalDeviceView logicalDevice);
	};
}
