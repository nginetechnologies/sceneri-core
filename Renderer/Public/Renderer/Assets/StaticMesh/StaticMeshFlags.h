#pragma once

#include <Common/EnumFlagOperators.h>

namespace ngine::Rendering
{
	enum class StaticMeshFlags : uint8
	{
		AllowCpuVertexAccess = 1 << 0,
		//! Whether the mesh was cloned from another
		IsClone = 1 << 1,
		WasLoaded = 1 << 2,
		FailedLoading = 1 << 3,
		// True if either of the two bits are set
		HasFinishedLoading = WasLoaded | FailedLoading
	};
	ENUM_FLAG_OPERATORS(StaticMeshFlags);
}
