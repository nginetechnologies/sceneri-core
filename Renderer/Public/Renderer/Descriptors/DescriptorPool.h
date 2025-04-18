#pragma once

#include <Common/EnumFlags.h>
#include <Common/EnumFlagOperators.h>
#include <Common/Memory/Containers/ForwardDeclarations/ArrayView.h>

#include <Renderer/Descriptors/DescriptorType.h>

#include <Renderer/Descriptors/DescriptorPoolView.h>

namespace ngine::Rendering
{
	struct LogicalDeviceView;

	struct DescriptorPool : public DescriptorPoolView
	{
		struct Size
		{
			Size(const DescriptorType type, const uint32 count)
				: m_type((uint32)type)
				, m_count(count)
			{
			}
		protected:
			uint32 m_type;
			uint32 m_count;
		};

		enum class CreationFlags : uint8
		{
			SupportIndividualFree = 1 << 0,
			UpdateAfterBind = 1 << 1
		};

		DescriptorPool() = default;
		DescriptorPool(
			const LogicalDeviceView logicalDevice,
			const uint32 maximumDescriptorSetCount,
			const ArrayView<const Size, uint8> poolSizes,
			const CreationFlags flags
		);
		DescriptorPool(const DescriptorPool&) = delete;
		DescriptorPool& operator=(const DescriptorPool&) = delete;
		DescriptorPool([[maybe_unused]] DescriptorPool&& other)
		{
#if RENDERER_VULKAN
			m_pPool = other.m_pPool;
			other.m_pPool = 0;
#endif
		}
		DescriptorPool& operator=(DescriptorPool&&);
		~DescriptorPool();

		void Destroy(const LogicalDeviceView logicalDevice);
	};

	ENUM_FLAG_OPERATORS(DescriptorPool::CreationFlags);
}
