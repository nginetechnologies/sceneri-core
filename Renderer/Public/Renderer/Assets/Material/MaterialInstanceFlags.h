#pragma once

#include <Common/EnumFlagOperators.h>

namespace ngine::Rendering
{
	enum class MaterialInstanceFlags : uint8
	{
		//! Whether the material instance was cloned from another
		IsClone = 1 << 0,
		WasLoaded = 1 << 1,
		FailedLoading = 1 << 2,
		// True if either of the two bits are set
		HasFinishedLoading = WasLoaded | FailedLoading
	};
	ENUM_FLAG_OPERATORS(MaterialInstanceFlags);
}
