#include <Renderer/Buffers/DeviceMemory.h>
#include <Renderer/Buffers/Buffer.h>
#include <Renderer/Devices/LogicalDeviceView.h>

#include <Renderer/Vulkan/Includes.h>
#include <Renderer/Metal/Includes.h>
#include <Renderer/Metal/GetStorageMode.h>

#include <Common/Memory/Allocators/Allocate.h>
#include <Common/Platform/Unused.h>
#include <Common/Math/Range.h>

namespace ngine::Rendering
{
	DeviceMemory::DeviceMemory(
		const LogicalDeviceView logicalDevice,
		const size allocationSize,
		[[maybe_unused]] const uint32 memoryTypeIndex,
		[[maybe_unused]] const EnumFlags<MemoryFlags> memoryFlags
	)
	{
#if RENDERER_VULKAN
		const VkMemoryAllocateFlagsInfo memoryAllocateFlagsInfo{
			VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
			nullptr,
			VkMemoryAllocateFlags(VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR * memoryFlags.IsSet(MemoryFlags::AllocateDeviceAddress)),
			0
		};

		const VkMemoryAllocateInfo allocationInfo =
			{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, &memoryAllocateFlagsInfo, allocationSize, memoryTypeIndex};

		[[maybe_unused]] const VkResult allocationResult = vkAllocateMemory(logicalDevice, &allocationInfo, nullptr, &m_pDeviceMemory);
		Assert(allocationResult == VK_SUCCESS || allocationResult == VK_ERROR_OUT_OF_DEVICE_MEMORY);

#if PROFILE_BUILD
		if (allocationResult == VK_SUCCESS)
		{
			m_size = allocationSize;
			Memory::ReportGraphicsAllocation(allocationSize);
		}
#endif

#elif RENDERER_METAL
		MTLHeapDescriptor* heapDescriptor = [MTLHeapDescriptor new];
		heapDescriptor.type = MTLHeapTypePlacement;
		heapDescriptor.storageMode = Metal::GetStorageMode(memoryFlags);

		// TODO: Set MTLCPUCacheModeWriteCombined if we never read from CPU
		heapDescriptor.cpuCacheMode = MTLCPUCacheModeDefaultCache;
		heapDescriptor.hazardTrackingMode = MTLHazardTrackingModeTracked; // TODO: Switch to untracked
		heapDescriptor.size = allocationSize;

		const id<MTLHeap> heap = [(id<MTLDevice>)logicalDevice newHeapWithDescriptor:heapDescriptor];
		if (heap != nil)
		{
#if PROFILE_BUILD
			m_size = allocationSize;
#endif
			Memory::ReportGraphicsAllocation(allocationSize);
		}

		m_pDeviceMemory = heap;

#else
		Assert(false, "TODO");
		UNUSED(logicalDevice);
		UNUSED(allocationSize);
		UNUSED(memoryTypeIndex);
#endif
	}

	DeviceMemory& DeviceMemory::operator=([[maybe_unused]] DeviceMemory&& other) noexcept
	{
		Assert(!IsValid(), "Destroy must be called before the buffer is destroyed!");
#if RENDERER_VULKAN || RENDERER_METAL
		m_pDeviceMemory = other.m_pDeviceMemory;
		other.m_pDeviceMemory = 0;
#endif

#if PROFILE_BUILD
		m_size = other.m_size;
		other.m_size = 0;
#endif
		return *this;
	}

	DeviceMemory::~DeviceMemory()
	{
		Assert(!IsValid(), "Destroy must be called before the buffer is destroyed!");
	}

	void DeviceMemory::Destroy([[maybe_unused]] const LogicalDeviceView logicalDevice)
	{
#if PROFILE_BUILD
		if (IsValid())
		{
			Memory::ReportGraphicsDeallocation(GetSize());
		}
		m_size = 0;
#endif

#if RENDERER_VULKAN
		vkFreeMemory(logicalDevice, m_pDeviceMemory, nullptr);
		m_pDeviceMemory = 0;
#elif RENDERER_METAL
		m_pDeviceMemory = nullptr;
#endif
	}
}
