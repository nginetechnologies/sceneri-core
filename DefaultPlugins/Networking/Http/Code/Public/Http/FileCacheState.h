#pragma once

#include <Common/Math/CoreNumericTypes.h>

namespace ngine::Networking::HTTP
{
	enum class FileCacheState : uint8
	{
		//! File state hasn't been determined yet
		Unknown,
		//! File in cache is valid and does not need to be redownloaded
		Valid,
		//! File in cache is out of date and should be redownloaded
		OutOfDate,
		//! File cache state validation failed, perhaps the server is down (use the local file for now)
		ValidationFailed
	};
}
