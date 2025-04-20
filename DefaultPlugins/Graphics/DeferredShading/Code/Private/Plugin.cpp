#include "Plugin.h"

#include "PBRLightingStage.h"
#include "ShadowsStage.h"
#include "TilePopulationStage.h"
#include "SSAOStage.h"
#include "PostProcessStage.h"
#include "MaterialIdentifiersStage.h"

#include <Common/System/Query.h>
#include <Renderer/Renderer.h>

namespace ngine::DeferredShading
{
	void Plugin::OnLoaded(Application&)
	{
		if (const Optional<Rendering::Renderer*> pRenderer = System::Find<Rendering::Renderer>())
		{
			pRenderer->GetStageCache().FindOrRegisterAsset(Rendering::ShadowsStage::Guid, UnicodeString(MAKE_UNICODE_LITERAL("Shadows")));
			pRenderer->GetStageCache().FindOrRegisterAsset(
				Rendering::TilePopulationStage::Guid,
				UnicodeString(MAKE_UNICODE_LITERAL("Tile Population")),
				Rendering::StageFlags::Hidden
			);
			pRenderer->GetStageCache()
				.FindOrRegisterAsset(Rendering::SSAOStage::Guid, UnicodeString(MAKE_UNICODE_LITERAL("SSAO")), Rendering::StageFlags::Hidden);
			pRenderer->GetStageCache().FindOrRegisterAsset(
				Rendering::PostProcessStage::Guid,
				UnicodeString(MAKE_UNICODE_LITERAL("PostProcess")),
				Rendering::StageFlags::Hidden
			);
			pRenderer->GetStageCache()
				.FindOrRegisterAsset(Rendering::PBRLightingStage::Guid, UnicodeString(MAKE_UNICODE_LITERAL("Deferred Lighting")));
			pRenderer->GetStageCache()
				.FindOrRegisterAsset(Rendering::MaterialIdentifiersStage::Guid, UnicodeString(MAKE_UNICODE_LITERAL("Material Identifiers")));
		}
	}
}

#if PLUGINS_IN_EXECUTABLE
[[maybe_unused]] static bool entryPoint = ngine::Plugin::Register<ngine::DeferredShading::Plugin>();
#else
extern "C" DEFERREDSHADING_EXPORT_API ngine::Plugin* InitializePlugin(ngine::Application& application)
{
	UNUSED(engine);

	return new ngine::DeferredShading::Plugin(application);
}
#endif
