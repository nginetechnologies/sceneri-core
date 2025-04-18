#pragma once

#include <Common/Math/CoreNumericTypes.h>
#include <Common/Memory/CountBits.h>

#include <Common/EnumFlagOperators.h>

namespace ngine::Rendering
{
	enum class QueueFamily : uint8
	{
		Graphics = 1 << 0,
		First = Graphics,
		Transfer = 1 << 1,
		Compute = 1 << 2,
		End = 1 << 3,
		Count = Memory::GetBitWidth<uint8>(static_cast<uint8>(Compute))
	};
	ENUM_FLAG_OPERATORS(QueueFamily);

#if RENDERER_VULKAN
	using QueueFamilyIndex = uint32;
#else
	using QueueFamilyIndex = uint16;
#endif
}
