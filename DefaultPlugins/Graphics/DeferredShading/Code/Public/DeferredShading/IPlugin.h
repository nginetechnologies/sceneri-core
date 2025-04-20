#pragma once

#include <Common/Plugin/Plugin.h>
#include <Common/Guid.h>

namespace ngine::DeferredShading
{
	struct IPlugin : public ngine::Plugin
	{
		inline static constexpr Guid Guid = "B7921721-4029-47CE-873C-5D356D604AA9"_guid;
	};
}
