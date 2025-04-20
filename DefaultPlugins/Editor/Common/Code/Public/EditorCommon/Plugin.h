#pragma once

#include <Common/Plugin/Plugin.h>
#include <Common/Guid.h>

namespace ngine::Editor::Common
{
	struct Plugin final : public ngine::Plugin
	{
		inline static constexpr ngine::Guid Guid = "4F6B545D-F826-4BAE-9A41-CD5149979D8D"_guid;

		Plugin(Application&)
		{
		}
		virtual ~Plugin() = default;

		// IPlugin
		virtual void OnLoaded(Application& application) override;
		// ~IPlugin
	};
}
