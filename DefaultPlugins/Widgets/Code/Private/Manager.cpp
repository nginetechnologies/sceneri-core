#include "Manager.h"

#include <Engine/Asset/AssetManager.h>
#include <Common/System/Query.h>

#include <Common/Memory/Containers/Vector.h>

namespace ngine::Widgets
{
	Manager::Manager(Application&)
		: m_stylesheetCache(System::Get<Asset::Manager>())
	{
	}

	Manager::~Manager() = default;
}

#if PLUGINS_IN_EXECUTABLE
[[maybe_unused]] static bool entryPoint = ngine::Plugin::Register<ngine::Widgets::Manager>();
#else
extern "C" VISUALDEBUG_EXPORT_API ngine::Plugin* InitializePlugin(ngine::Application& application)
{
	return new ngine::Widgets::Manager(application);
}
#endif
