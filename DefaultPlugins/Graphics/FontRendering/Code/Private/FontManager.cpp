#include "Manager.h"
#include "Font.h"
#include "Stages/DrawTextStage.h"

#include <Common/Memory/OffsetOf.h>
#include <Common/IO/AccessModeFlags.h>
#include <Common/EnumFlags.h>
#include <Common/Reflection/Registry.inl>
#include <Common/Reflection/Type.h>

#include <Engine/Asset/AssetManager.h>
#include <Common/System/Query.h>

#include <Renderer/Renderer.h>

namespace ngine::Font
{
	Manager::Manager(Application&)
		: m_cache(System::Get<Asset::Manager>())
	{
	}

	Manager::~Manager()
	{
	}

	void Manager::OnLoaded(Application&)
	{
		System::Get<Rendering::Renderer>().GetStageCache().FindOrRegisterAsset(
			DrawTextStage::TypeGuid,
			UnicodeString(MAKE_UNICODE_LITERAL("Draw Text"))
		);
	}
}

namespace ngine::Font
{
	[[maybe_unused]] const bool wasFontPointTypeRegistered = Reflection::Registry::RegisterType<ngine::Font::Point>();
}

#if PLUGINS_IN_EXECUTABLE
[[maybe_unused]] static bool entryPoint = ngine::Plugin::Register<ngine::Font::Manager>();
#else
extern "C" VISUALDEBUG_EXPORT_API ngine::Plugin* InitializePlugin(ngine::Application& application)
{
	return new ngine::Font::Manager(engine);
}
#endif
