#pragma once

namespace ngine::Rendering
{
#if 0 // PLATFORM_MOBILE
	using Index = uint16;
#else
	using Index = uint32;
#endif
}
