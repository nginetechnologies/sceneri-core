#pragma once

#include <RenderDebug/IPlugin.h>

namespace ngine::Rendering::Debug
{
	struct Plugin final : public IPlugin
	{
		Plugin(Application&)
		{
		}
		virtual ~Plugin() = default;

		// IPlugin
		virtual void OnLoaded(Application& application) override;
		// ~IPlugin
	};
}
