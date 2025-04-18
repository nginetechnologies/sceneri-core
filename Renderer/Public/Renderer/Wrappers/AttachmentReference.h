#pragma once

#include <Renderer/ImageLayout.h>

namespace ngine::Rendering
{
	struct AttachmentReference
	{
		uint32 m_index = 0;
		ImageLayout m_layout = ImageLayout::Undefined;
	};
}
