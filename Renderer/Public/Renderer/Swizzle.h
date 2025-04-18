#pragma once

namespace ngine::Rendering
{
	enum class Swizzle : uint8
	{
		FirstChannel,
		Red = FirstChannel,
		Green,
		Blue,
		Alpha,
		LastChannel = Alpha,
		Zero,
		One
	};
}
