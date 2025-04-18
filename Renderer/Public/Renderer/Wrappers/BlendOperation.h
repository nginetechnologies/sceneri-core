#pragma once

namespace ngine::Rendering
{
	enum class BlendOperation : uint32
	{
		Add = 0,
		Subtract = 1,
		ReverseSubtract = 2,
		Minimum = 3,
		Maximum = 4,
	};
}
