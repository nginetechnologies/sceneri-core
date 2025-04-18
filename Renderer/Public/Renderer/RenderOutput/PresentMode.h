#pragma once

namespace ngine::Rendering
{
	enum class PresentMode : uint8
	{
		Immediate,
		Mailbox,
		FirstInFirstOut,
	};
}
