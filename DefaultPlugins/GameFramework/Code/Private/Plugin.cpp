#include "Plugin.h"
#include "Tags.h"

#include <Engine/Tag/TagRegistry.h>

#include <Common/System/Query.h>

namespace ngine::GameFramework
{
	void Plugin::OnLoaded(Application&)
	{
		Tag::Registry& tagRegistry = System::Get<Tag::Registry>();
		tagRegistry.FindOrRegister(Tags::PlayerTagGuid, MAKE_UNICODE_LITERAL("Player"), "#06B6D466"_colorf, Tag::Flags::VisibleToUI);
		tagRegistry.FindOrRegister(Tags::VehicleTagGuid, MAKE_UNICODE_LITERAL("Vehicle"), "#F9731666"_colorf, Tag::Flags::VisibleToUI);
		tagRegistry.FindOrRegister(
			Tags::InteractableObjectTagGuid,
			MAKE_UNICODE_LITERAL("Interactable Object"),
			"#F43F5E66"_colorf,
			Tag::Flags::Removable | Tag::Flags::Addable | Tag::Flags::VisibleToUI
		);
	}
}

#if PLUGINS_IN_EXECUTABLE
[[maybe_unused]] static bool entryPoint = ngine::Plugin::Register<ngine::GameFramework::Plugin>();
#else
extern "C" GAMEFRAMEWORK_EXPORT_API ngine::Plugin* InitializePlugin(ngine::Application& application)
{
	return new ngine::GameFramework::Plugin(application);
}
#endif
