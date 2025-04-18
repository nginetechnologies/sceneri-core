#pragma once

#include <Common/EnumFlagOperators.h>
#include <Common/Math/CoreNumericTypes.h>

namespace ngine::Rendering
{
	enum class ImageAspectFlags
#if RENDERER_VULKAN
		: uint32
#else
		: uint8
#endif
	{
		/* Maps to VkImageAspectFlags */
		Color = 1 << 0,
		Depth = 1 << 1,
		Stencil = 1 << 2,
		DepthStencil = Depth | Stencil,
		Plane0 = 0x00000010,
		Plane1 = 0x00000020,
		Plane2 = 0x00000040
	};

	ENUM_FLAG_OPERATORS(ImageAspectFlags);
}
