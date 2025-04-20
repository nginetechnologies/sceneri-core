#include "Plugin.h"

#include "DrawNormalsStage.h"
#include "DrawWireframeStage.h"

#include <Common/System/Query.h>
#include <Renderer/Renderer.h>

namespace ngine::Rendering::Debug
{
	void Plugin::OnLoaded(Application&)
	{
		System::Get<Rendering::Renderer>().GetStageCache().FindOrRegisterAsset(
			DrawNormalsStage::Guid,
			UnicodeString(MAKE_UNICODE_LITERAL("Draw Normals"))
		);
		System::Get<Rendering::Renderer>().GetStageCache().FindOrRegisterAsset(
			DrawTangentsStage::Guid,
			UnicodeString(MAKE_UNICODE_LITERAL("Draw Tangents"))
		);
		System::Get<Rendering::Renderer>().GetStageCache().FindOrRegisterAsset(
			DrawBitangentsStage::Guid,
			UnicodeString(MAKE_UNICODE_LITERAL("Draw Bitangents"))
		);
		System::Get<Rendering::Renderer>().GetStageCache().FindOrRegisterAsset(
			DrawWireframeStage::Guid,
			UnicodeString(MAKE_UNICODE_LITERAL("Draw Wireframe"))
		);
	}
}

#if PLUGINS_IN_EXECUTABLE
[[maybe_unused]] static bool entryPoint = ngine::Plugin::Register<ngine::Rendering::Debug::Plugin>();
#else
extern "C" RENDERDEBUG_EXPORT_API ngine::Plugin* InitializePlugin(ngine::Application& application)
{
	return new ngine::Rendering::Debug::Plugin(application);
}
#endif
