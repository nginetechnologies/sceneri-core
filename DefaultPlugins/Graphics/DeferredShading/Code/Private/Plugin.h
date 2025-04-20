#pragma once

#include <DeferredShading/IPlugin.h>

namespace ngine::DeferredShading
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
