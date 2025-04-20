#pragma once

#include <Common/Plugin/Plugin.h>
#include <Common/Guid.h>

namespace ngine::Rendering::Debug
{
	struct IPlugin : public ngine::Plugin
	{
		inline static constexpr Guid Guid = "B8C8DFE5-D303-4E2F-A82E-15AE4EFA1A4B"_guid;
	};
}
