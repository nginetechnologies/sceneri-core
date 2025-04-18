#pragma once

#include <Common/EnumFlagOperators.h>

namespace ngine::Rendering
{
	enum class LoadedTextureFlags : uint8
	{
		IsDummy = 1 << 0,
		WasResized = 1 << 1,
		HasDepth = 1 << 2,
		HasStencil = 1 << 3
	};

	ENUM_FLAG_OPERATORS(LoadedTextureFlags);
}
