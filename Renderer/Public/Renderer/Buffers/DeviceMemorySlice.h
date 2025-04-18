#pragma once

#include <Renderer/Buffers/DeviceMemoryView.h>

namespace ngine::Rendering
{
	struct DeviceMemorySlice
	{
		DeviceMemoryView m_memoryView;
		size m_offset;
	};
}
