#pragma once

namespace ngine::Rendering
{
	enum class SampleCount
#if RENDERER_VULKAN
		: uint32
#else
		: uint8
#endif
	{
		One = 1,
		Two = 2,
		Four = 4,
		Eight = 8,
		Sixteen = 16,
		ThirtyTwo = 32,
		SixtyFour = 64
	};
}
