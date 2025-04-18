#pragma once

#include <Renderer/GetSupportedAccessFlags.h>
#include <Renderer/Wrappers/ImageView.h>
#include <Renderer/Wrappers/ImageSubresourceRange.h>
#include <Renderer/Devices/QueueFamily.h>

#include <Common/EnumFlags.h>

namespace ngine::Rendering
{
	struct ImageMemoryBarrier
	{
#if RENDERER_VULKAN
	protected:
		uint32 type = 45;
		const void* pNext = nullptr;
#endif
	public:
		ImageMemoryBarrier() = default;

		ImageMemoryBarrier(
			const EnumFlags<AccessFlags> sourceAccessMask,
			const EnumFlags<AccessFlags> targetAccessMask,
			const ImageLayout oldLayout,
			const ImageLayout newLayout,
			const QueueFamilyIndex sourceQueueFamilyIndex,
			const QueueFamilyIndex targetQueueFamilyIndex,
			const ImageView image,
			const ImageSubresourceRange subresourceRange
		)
			: m_sourceAccessMask(sourceAccessMask)
			, m_targetAccessMask(targetAccessMask)
			, m_oldLayout(oldLayout)
			, m_newLayout(newLayout)
			, m_sourceQueueFamilyIndex(sourceQueueFamilyIndex)
			, m_targetQueueFamilyIndex(targetQueueFamilyIndex)
			, m_image(image)
			, m_subresourceRange(subresourceRange)
		{
			Assert(
				GetSupportedAccessFlags(oldLayout).AreAllSet(sourceAccessMask),
				"Source flags must be compatible with what's supported for the previous layout"
			);
			Assert(
				GetSupportedAccessFlags(newLayout).AreAllSet(targetAccessMask),
				"Target flags must be compatible with what's supported for the new layout"
			);
		}

		ImageMemoryBarrier(
			const EnumFlags<AccessFlags> sourceAccessMask,
			const EnumFlags<AccessFlags> targetAccessMask,
			const ImageLayout oldLayout,
			const ImageLayout newLayout,
			const ImageView image,
			const ImageSubresourceRange subresourceRange
		)
			: ImageMemoryBarrier{
					sourceAccessMask,
					targetAccessMask,
					oldLayout,
					newLayout,
					(QueueFamilyIndex)~QueueFamilyIndex(0u),
					(QueueFamilyIndex)~QueueFamilyIndex(0u),
					image,
					subresourceRange
				}
		{
		}

		EnumFlags<AccessFlags> m_sourceAccessMask;
		EnumFlags<AccessFlags> m_targetAccessMask;
		ImageLayout m_oldLayout;
		ImageLayout m_newLayout;
		QueueFamilyIndex m_sourceQueueFamilyIndex;
		QueueFamilyIndex m_targetQueueFamilyIndex;
		ImageView m_image;
		ImageSubresourceRange m_subresourceRange;
	};
}
