#pragma once

namespace ngine::Rendering
{
	enum class AddressMode : uint8
	{
		Repeat,
		RepeatMirrored,
		ClampToEdge,
		ClampToBorder,
		ClampToEdgeMirrored
	};
}
