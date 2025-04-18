#pragma once

#include <Renderer/Vulkan/ForwardDeclares.h>

#include <Common/Platform/ForceInline.h>
#include <Common/Platform/TrivialABI.h>

namespace ngine::Rendering
{
	struct LogicalDeviceView;
	struct LogicalDevice;
	struct DescriptorSetLayoutView;
	struct DescriptorSet;

	struct TRIVIAL_ABI DescriptorPoolView
	{
#if RENDERER_VULKAN
		[[nodiscard]] operator VkDescriptorPool() const
		{
			return m_pPool;
		}
#endif

		[[nodiscard]] bool IsValid() const
		{
#if RENDERER_VULKAN
			return m_pPool != 0;
#else
			return false;
#endif
		}

		[[nodiscard]] bool AllocateDescriptorSets(
			LogicalDevice& logicalDevice,
			const ArrayView<const DescriptorSetLayoutView, uint8> layouts,
			const ArrayView<DescriptorSet, uint8> descriptorSetsOut,
			const ArrayView<const uint32, uint8> maximumVariableDescriptorCount = {}
		) const;
		void FreeDescriptorSets(LogicalDevice& logicalDevice, const ArrayView<DescriptorSet> descriptorSets) const;
	protected:
#if RENDERER_VULKAN
		VkDescriptorPool m_pPool = 0;
#endif
	};
}
