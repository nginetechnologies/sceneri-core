#pragma once

namespace ngine::Rendering
{
	enum class AttachmentStoreType
#if RENDERER_VULKAN
		: uint32
#else
		: uint8
#endif
	{
		Store,
		Undefined
	};
}
