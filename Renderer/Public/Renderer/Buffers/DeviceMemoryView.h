#pragma once

#include <Common/Platform/ForceInline.h>
#include <Common/Platform/LifetimeBound.h>
#include <Common/Platform/TrivialABI.h>

#include <Renderer/Vulkan/ForwardDeclares.h>
#include <Renderer/Metal/ForwardDeclares.h>

namespace ngine::Rendering
{
#define RENDERER_HAS_DEVICE_MEMORY (RENDERER_VULKAN || RENDERER_METAL)

	struct TRIVIAL_ABI DeviceMemoryView
	{
		DeviceMemoryView() = default;

#if RENDERER_VULKAN
		DeviceMemoryView(const VkDeviceMemory pDeviceMemory)
			: m_pDeviceMemory(pDeviceMemory)
		{
		}

		[[nodiscard]] operator VkDeviceMemory() const LIFETIME_BOUND
		{
			return m_pDeviceMemory;
		}
#elif RENDERER_METAL
		DeviceMemoryView(id<MTLHeap> heap)
			: m_pDeviceMemory(heap)
		{
		}

		[[nodiscard]] operator id<MTLHeap>() const LIFETIME_BOUND
		{
			return m_pDeviceMemory;
		}
#endif
		DeviceMemoryView(const DeviceMemoryView&) = default;
		DeviceMemoryView& operator=(const DeviceMemoryView&) = default;
		DeviceMemoryView([[maybe_unused]] DeviceMemoryView&& other)
#if RENDERER_VULKAN || RENDERER_METAL
			: m_pDeviceMemory(other.m_pDeviceMemory)
#endif
		{
#if RENDERER_VULKAN || RENDERER_METAL
			other.m_pDeviceMemory = 0;
#endif
		}
		DeviceMemoryView& operator=([[maybe_unused]] DeviceMemoryView&& other)
		{
#if RENDERER_VULKAN || RENDERER_METAL
			m_pDeviceMemory = other.m_pDeviceMemory;
			other.m_pDeviceMemory = 0;
#endif
			return *this;
		}

		[[nodiscard]] bool IsValid() const
		{
#if RENDERER_VULKAN || RENDERER_METAL
			return m_pDeviceMemory != 0;
#else
			return false;
#endif
		}

		[[nodiscard]] bool operator==([[maybe_unused]] const DeviceMemoryView& other) const
		{
#if RENDERER_VULKAN || RENDERER_METAL
			return m_pDeviceMemory == other.m_pDeviceMemory;
#else
			return false;
#endif
		}
		[[nodiscard]] bool operator!=(const DeviceMemoryView& other) const
		{
			return !operator==(other);
		}
	protected:
#if RENDERER_VULKAN
		VkDeviceMemory m_pDeviceMemory = 0;
#elif RENDERER_METAL
		id<MTLHeap> m_pDeviceMemory{nullptr};
#endif
	};
}
