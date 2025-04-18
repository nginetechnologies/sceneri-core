#pragma once

#include <Renderer/Buffers/DeviceMemoryView.h>
#include <Renderer/Devices/MemoryFlags.h>

#include <Common/EnumFlags.h>

namespace ngine::Rendering
{
	struct LogicalDeviceView;

	struct DeviceMemory : public DeviceMemoryView
	{
		DeviceMemory() = default;
		DeviceMemory(
			const LogicalDeviceView logicalDevice, const size size, const uint32 memoryTypeIndex, const EnumFlags<MemoryFlags> memoryFlags
		);
		DeviceMemory(const DeviceMemory&) = delete;
		DeviceMemory& operator=(const DeviceMemory&) = delete;
		DeviceMemory([[maybe_unused]] DeviceMemory&& other) noexcept
		{
#if RENDERER_VULKAN || RENDERER_METAL
			m_pDeviceMemory = other.m_pDeviceMemory;
			other.m_pDeviceMemory = 0;
#endif

#if PROFILE_BUILD
			m_size = other.m_size;
			other.m_size = 0;
#endif
		}
		DeviceMemory& operator=(DeviceMemory&& other) noexcept;
		~DeviceMemory();
		void Destroy(const LogicalDeviceView logicalDevice);

#if PROFILE_BUILD
		[[nodiscard]] size GetSize() const
		{
			return m_size;
		}
#endif
	private:
#if PROFILE_BUILD
		size m_size = 0;
#endif
	};
}
