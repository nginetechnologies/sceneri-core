#pragma once

namespace ngine::Rendering
{
	enum class AttachmentLoadType
#if RENDERER_VULKAN
		: uint32
#else
		: uint8
#endif
	{
		LoadExisting,
		Clear,
		Undefined
	};
}
