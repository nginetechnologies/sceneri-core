#include "Plugin.h"

namespace ngine::Editor::Common
{
	void Plugin::OnLoaded(Application&)
	{
	}
}

#if PLUGINS_IN_EXECUTABLE
[[maybe_unused]] static bool entryPoint = ngine::Plugin::Register<ngine::Editor::Common::Plugin>();
#else
extern "C" EDITORCORE_EXPORT_API ngine::Plugin* InitializePlugin(ngine::Application& application)
{
	return new ngine::Editor::Common::Plugin(application);
}
#endif
