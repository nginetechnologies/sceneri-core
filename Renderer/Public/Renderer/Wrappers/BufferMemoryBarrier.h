#pragma once

#include <Renderer/AccessFlags.h>
#include <Renderer/Buffers/BufferView.h>
#include <Renderer/Devices/QueueFamily.h>

namespace ngine::Rendering
{
	struct BufferMemoryBarrier
	{
#if RENDERER_VULKAN
	protected:
		uint32 type = 44;
		const void* pNext = nullptr;
#endif
	public:
		BufferMemoryBarrier(
			const AccessFlags sourceAccessMask,
			const AccessFlags targetAccessMask,
			const QueueFamilyIndex sourceQueueFamilyIndex,
			const QueueFamilyIndex targetQueueFamilyIndex,
			const BufferView buffer,
			const uint64 offset,
			const uint64 size
		)
			: m_sourceAccessMask(sourceAccessMask)
			, m_targetAccessMask(targetAccessMask)
			, m_sourceQueueFamilyIndex(sourceQueueFamilyIndex)
			, m_targetQueueFamilyIndex(targetQueueFamilyIndex)
			, m_buffer(buffer)
			, m_offset(offset)
			, m_size(size)
		{
		}

		BufferMemoryBarrier(
			const AccessFlags sourceAccessMask,
			const AccessFlags targetAccessMask,
			const BufferView buffer,
			const uint64 offset,
			const uint64 size
		)
			: m_sourceAccessMask(sourceAccessMask)
			, m_targetAccessMask(targetAccessMask)
#if RENDERER_VULKAN
			, m_sourceQueueFamilyIndex(~0u)
			, m_targetQueueFamilyIndex(~0u)
#else
			, m_sourceQueueFamilyIndex((QueueFamilyIndex)~QueueFamilyIndex(0u))
			, m_targetQueueFamilyIndex((QueueFamilyIndex)~QueueFamilyIndex(0u))
#endif
			, m_buffer(buffer)
			, m_offset(offset)
			, m_size(size)
		{
		}

		AccessFlags m_sourceAccessMask;
		AccessFlags m_targetAccessMask;
#if RENDERER_VULKAN
		uint32 m_sourceQueueFamilyIndex;
		uint32 m_targetQueueFamilyIndex;
#else
		QueueFamilyIndex m_sourceQueueFamilyIndex;
		QueueFamilyIndex m_targetQueueFamilyIndex;
#endif
		BufferView m_buffer;
		uint64 m_offset;
		uint64 m_size;
	};
}
