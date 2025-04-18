#pragma once

namespace ngine::Rendering
{
	enum class ImageFlags : uint16
	{
		Cubemap = 16,
		CreateDisjoint = 0x00000200,
	};
}
