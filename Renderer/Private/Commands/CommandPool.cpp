#include <Renderer/Commands/CommandPool.h>
#include <Renderer/Commands/CommandBuffer.h>

#include "Devices/LogicalDevice.h"
#include "Devices/PhysicalDevice.h"

#include <Renderer/Vulkan/Includes.h>

namespace ngine::Rendering
{
#if RENDERER_VULKAN
	[[nodiscard]] inline VkCommandPool
	CreateCommandPool(const LogicalDeviceView logicalDevice, const uint32 queueFamilyIndex, const EnumFlags<CommandPool::Flags> flags)
	{
		const VkCommandPoolCreateInfo poolInfo = {
			VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			nullptr,
			static_cast<VkCommandPoolCreateFlags>(flags.GetUnderlyingValue()),
			queueFamilyIndex
		};

		VkCommandPool pCommandPool;
		vkCreateCommandPool(logicalDevice, &poolInfo, nullptr, &pCommandPool);
		return pCommandPool;
	}
#endif

	CommandPool::CommandPool(
		[[maybe_unused]] const LogicalDeviceView logicalDevice,
		[[maybe_unused]] const uint32 queueFamilyIndex,
		[[maybe_unused]] const EnumFlags<Flags> flags
	)
#if RENDERER_VULKAN
		: CommandPoolView(CreateCommandPool(logicalDevice, queueFamilyIndex, flags))
#endif
	{
	}

	CommandPool& CommandPool::operator=([[maybe_unused]] CommandPool&& other) noexcept
	{
		Assert(!IsValid(), "Destroy must have been called!");
#if RENDERER_VULKAN
		m_pCommandPool = other.m_pCommandPool;
		other.m_pCommandPool = 0;
#endif
		return *this;
	}

	CommandPool::~CommandPool()
	{
		Assert(!IsValid(), "Destroy must have been called!");
	}

	void CommandPool::Destroy([[maybe_unused]] const LogicalDeviceView logicalDevice)
	{
#if RENDERER_VULKAN
		vkDestroyCommandPool(logicalDevice, m_pCommandPool, nullptr);
		m_pCommandPool = 0;
#elif RENDERER_HAS_COMMAND_POOL
		UNUSED(logicalDevice);
		Assert(false, "TODO");
#endif
	}

#if RENDERER_HAS_COMMAND_POOL
	bool CommandPoolView::Reset(const LogicalDeviceView logicalDevice, const ResetFlags flags) const
	{
#if RENDERER_VULKAN
		return vkResetCommandPool(logicalDevice, m_pCommandPool, static_cast<VkCommandPoolResetFlagBits>(flags)) == VK_SUCCESS;
#else
		Assert(false, "TODO");
		UNUSED(logicalDevice);
		UNUSED(flags);
		return false;
#endif
	}

	void CommandPoolView::FreeCommandBuffers(const LogicalDeviceView logicalDevice, ArrayView<CommandBuffer, uint16> commandBuffers) const
	{
#if RENDERER_VULKAN
		vkFreeCommandBuffers(
			logicalDevice,
			m_pCommandPool,
			commandBuffers.GetSize(),
			reinterpret_cast<const VkCommandBuffer*>(commandBuffers.GetData())
		);
#else
		Assert(false, "TODO");
		UNUSED(logicalDevice);
#endif

		for (CommandBuffer& commandBuffer : commandBuffers)
		{
			commandBuffer.OnBufferFreed();
		}
	}

	void CommandPoolView::AllocateCommandBuffers(const LogicalDeviceView logicalDevice, ArrayView<CommandBuffer, uint16> commandBuffers) const
	{
#if RENDERER_VULKAN
		const VkCommandBufferAllocateInfo allocInfo =
			{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, m_pCommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, commandBuffers.GetSize()};

		vkAllocateCommandBuffers(logicalDevice, &allocInfo, reinterpret_cast<VkCommandBuffer_T**>((void*)commandBuffers.GetData()));
#else
		Assert(false, "TODO");
		UNUSED(logicalDevice);
		UNUSED(commandBuffers);
#endif
	}

	void
	CommandPoolView::ReallocateCommandBuffers(const LogicalDeviceView logicalDevice, ArrayView<CommandBuffer, uint16> commandBuffers) const
	{
		FreeCommandBuffers(logicalDevice, commandBuffers);
		AllocateCommandBuffers(logicalDevice, commandBuffers);
	}
#endif
}
