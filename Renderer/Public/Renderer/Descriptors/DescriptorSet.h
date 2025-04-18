#pragma once

#include <Common/EnumFlags.h>
#include <Common/Memory/Containers/ForwardDeclarations/ArrayView.h>

#include <Renderer/Descriptors/DescriptorSetView.h>
#include <Renderer/Descriptors/DescriptorSetLayoutView.h>

namespace ngine::Rendering
{
	struct LogicalDevice;
	struct DescriptorPoolView;

	struct DescriptorSet : public DescriptorSetView
	{
		DescriptorSet() = default;
#if RENDERER_VULKAN || RENDERER_METAL || RENDERER_WEBGPU
		DescriptorSet(const DescriptorSetView descriptorSet)
			: DescriptorSetView(descriptorSet)
		{
		}
#endif
		DescriptorSet(const DescriptorSet&) = delete;
		DescriptorSet& operator=(const DescriptorSet&) = delete;
		DescriptorSet([[maybe_unused]] DescriptorSet&& other)
		{
#if RENDERER_VULKAN || RENDERER_METAL || RENDERER_WEBGPU
			m_pDescriptorSet = other.m_pDescriptorSet;
			other.m_pDescriptorSet = 0;
#endif
		}
		DescriptorSet& operator=(DescriptorSet&&);
		~DescriptorSet();

		void Destroy(LogicalDevice& logicalDevice, const DescriptorPoolView pool);

		[[nodiscard]] DescriptorSetView AtomicLoad() const;
		void AtomicSwap(DescriptorSet& other);
	protected:
		friend DescriptorPoolView;
		void OnFreed()
		{
#if RENDERER_VULKAN || RENDERER_METAL || RENDERER_WEBGPU
			m_pDescriptorSet = 0;
#endif
		}
	};
}
