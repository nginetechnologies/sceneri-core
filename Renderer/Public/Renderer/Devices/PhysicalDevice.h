#pragma once

#include "QueueFamily.h"
#include "MemoryFlags.h"
#include "PhysicalDeviceFeatures.h"
#include "PhysicalDeviceView.h"

#include <Common/EnumFlags.h>
#include <Common/Assert/Assert.h>

#include <Common/Memory/Containers/Array.h>
#include <Common/Memory/Containers/FlatVector.h>
#include <Common/Memory/CountBits.h>

#include <Renderer/Vulkan/ForwardDeclares.h>
#include <Renderer/WebGPU/ForwardDeclares.h>
#include <Renderer/Metal/ForwardDeclares.h>

namespace ngine::Rendering
{
	struct InstanceView;

	struct PhysicalDevice final
	{
		static constexpr uint8 MaximumCount = 4;

		EnumFlags<PhysicalDeviceFeatures> m_supportedFeatures; // Moved out here so that we can access from LogicalDevice creation
		PhysicalDevice() = default;
		PhysicalDevice(const PhysicalDeviceView device, const InstanceView instance, const unsigned int rating);
		PhysicalDevice(const PhysicalDevice&) = delete;
		PhysicalDevice& operator=(const PhysicalDevice&) = delete;
		PhysicalDevice(PhysicalDevice&& other) = default;
		PhysicalDevice& operator=(PhysicalDevice&&) = default;
		~PhysicalDevice();

		void AddQueueFamily(const QueueFamily family, QueueFamilyIndex index)
		{
			QueueFamilyIndex& queueFamilyIndices = m_queueFamilyIndices[Memory::GetBitWidth<uint8>(static_cast<uint8>(family) - 1u)];
			queueFamilyIndices |= QueueFamilyIndex(1u << index);
		}

		[[nodiscard]] QueueFamilyIndex GetQueueFamily(const QueueFamily family) const
		{
			const QueueFamilyIndex queueFamilyIndices = m_queueFamilyIndices[Memory::GetBitWidth<uint8>(static_cast<uint8>(family) - 1u)];
			Assert(queueFamilyIndices != 0, "Queue family must be set to be retrieved!");
			return Memory::GetFirstSetIndex(queueFamilyIndices);
		}
		[[nodiscard]] QueueFamilyIndex GetQueueFamilyCount() const
		{
			return m_queueFamilyCount;
		}

		inline static constexpr size MaximumMemoryTypeCount = 32;
		using MemoryTypeSizeType = Memory::NumericSize<MaximumMemoryTypeCount>;

		[[nodiscard]] ArrayView<const EnumFlags<MemoryFlags>, MemoryTypeSizeType> GetMemoryTypes() const
		{
			return m_memoryTypes;
		}

		[[nodiscard]] PURE_LOCALS_AND_POINTERS MemoryTypeSizeType
		GetMemoryTypeIndex(const EnumFlags<MemoryFlags> flags, const uint32 typeFilter = Math::NumericLimits<uint32>::Max) const;

		[[nodiscard]] EnumFlags<PhysicalDeviceFeatures> GetSupportedFeatures() const
		{
			return m_supportedFeatures;
		}

		[[nodiscard]] operator PhysicalDeviceView() const
		{
			return m_device;
		}
		[[nodiscard]] bool IsValid() const
		{
			return m_device.IsValid();
		}

#if RENDERER_VULKAN
		[[nodiscard]] operator VkPhysicalDevice() const
		{
			return m_device;
		}
#elif RENDERER_METAL
		[[nodiscard]] operator id<MTLDevice>() const
		{
			return m_device;
		}
#elif RENDERER_WEBGPU
		[[nodiscard]] operator WGPUAdapter() const
		{
			return m_device;
		}
#endif

		[[nodiscard]] uint32 GetRating() const
		{
			return m_rating;
		}
		[[nodiscard]] bool SupportsSwapchain() const
		{
			return m_canSupportSwapchain;
		}
	protected:
		PhysicalDeviceView m_device;
		uint32 m_rating;
		bool m_canSupportSwapchain{false};

		Array<QueueFamilyIndex, static_cast<uint8>(QueueFamily::Count)> m_queueFamilyIndices{Memory::Zeroed};
		QueueFamilyIndex m_queueFamilyCount{0};

		// EnumFlags<PhysicalDeviceFeatures> m_supportedFeatures;

		FlatVector<EnumFlags<MemoryFlags>, MaximumMemoryTypeCount> m_memoryTypes;
	};
};
