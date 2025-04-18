#pragma once

#include <Common/EnumFlagOperators.h>
#include <Common/Math/CoreNumericTypes.h>

namespace ngine::Rendering
{
	enum class UsageFlags : uint32
	{
		/* Maps to VkImageUsageFlags */
		TransferSource = 1,
		TransferDestination = 2,
		Sampled = 4,
		Storage = 8,
		ColorAttachment = 16,
		DepthStencilAttachment = 32,
		TransientAttachment = 64,
		InputAttachment = 128,
	};

	ENUM_FLAG_OPERATORS(UsageFlags);
}
