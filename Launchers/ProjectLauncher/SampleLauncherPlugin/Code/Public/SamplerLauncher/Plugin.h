#pragma once

#include <Common/Plugin/Plugin.h>
#include <Common/Guid.h>

namespace ngine::App::Core
{
	struct Plugin final : public ngine::Plugin
	{
		inline static constexpr Guid Guid = "84cb6ecd-1f00-4178-8434-5853cebb8601"_guid;

		Plugin(Application&)
		{
		}
		virtual ~Plugin() = default;

		// IPlugin
		virtual void OnLoaded(Application& application) override;
		// ~IPlugin
	};
}
