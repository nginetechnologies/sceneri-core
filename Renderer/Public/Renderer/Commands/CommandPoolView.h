#pragma once

#include <Renderer/Vulkan/ForwardDeclares.h>
#include <Common/Memory/Containers/ForwardDeclarations/ArrayView.h>
#include <Common/Platform/ForceInline.h>
#include <Common/Platform/TrivialABI.h>
#include <Common/Assert/Assert.h>

namespace ngine::Rendering
{
	struct PhysicalDevice;
	struct LogicalDeviceView;
	struct CommandBuffer;

	struct TRIVIAL_ABI CommandPoolView
	{
#define RENDERER_HAS_COMMAND_POOL RENDERER_VULKAN
		inline static constexpr bool IsSupported = RENDERER_HAS_COMMAND_POOL;

		CommandPoolView() = default;
#if RENDERER_VULKAN
		CommandPoolView(VkCommandPool pCommandPool)
			: m_pCommandPool(pCommandPool)
		{
		}
#endif

		enum class ResetFlags : uint8
		{
			/* maps to VkCommandPoolResetFlagBits */
			ReturnCommandBufferMemoryToPool = 1 << 0
		};

#if RENDERER_HAS_COMMAND_POOL
		[[nodiscard]] bool Reset(const LogicalDeviceView logicalDevice, const ResetFlags flags) const;
		void FreeCommandBuffers(const LogicalDeviceView logicalDevice, ArrayView<CommandBuffer, uint16> commandBuffers) const;
		void AllocateCommandBuffers(const LogicalDeviceView logicalDevice, ArrayView<CommandBuffer, uint16> commandBuffers) const;
		void ReallocateCommandBuffers(const LogicalDeviceView logicalDevice, ArrayView<CommandBuffer, uint16> commandBuffers) const;
#endif

		[[nodiscard]] bool IsValid() const
		{
#if RENDERER_VULKAN
			return m_pCommandPool != 0;
#else
			return false;
#endif
		}

		[[nodiscard]] bool operator==([[maybe_unused]] const CommandPoolView other) const
		{
#if RENDERER_VULKAN
			return m_pCommandPool == other.m_pCommandPool;
#else
			return false;
#endif
		}

		[[nodiscard]] bool operator!=(const CommandPoolView other) const
		{
			return !operator==(other);
			;
		}
	protected:
#if RENDERER_VULKAN
		VkCommandPool m_pCommandPool = 0;
#endif
	};
}
