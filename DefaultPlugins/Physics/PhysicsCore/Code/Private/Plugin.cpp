#include "Plugin.h"
#include "MaterialAsset.h"
#include "Material.h"

#include <Engine/Engine.h>
#include <Engine/Threading/JobManager.h>
#include <Engine/Asset/AssetManager.h>

#include <Common/Serialization/SerializedData.h>

#include <Common/System/Query.h>

#include "3rdparty/jolt/Jolt.h"
#include "3rdparty/jolt/RegisterTypes.h"
#include "3rdparty/jolt/Physics/PhysicsSettings.h"
#include "3rdparty/jolt/Core/Factory.h"

#include <cstdarg>

#if ENABLE_JOLT_DEBUG_RENDERER
#include "TestFramework/TestFramework.h"
#include "TestFramework/Renderer/Font.h"
#include "TestFramework/Renderer/DebugRendererImp.h"
#endif

namespace ngine::Physics
{
	Plugin::Plugin(Application&)
		: m_jobSystem(System::Get<Threading::JobManager>())
		, m_materialCache(System::Get<Asset::Manager>())
		, m_meshCache(System::Get<Asset::Manager>())
	{
	}

	void Plugin::OnLoaded([[maybe_unused]] Application& application)
	{
		JPH::Factory::sInstance = new JPH::Factory();
		JPH::RegisterTypes();

#if ENABLE_JOLT_DEBUG_RENDERER
		Engine& engine = static_cast<Engine&>(application);

		// Create renderer
		m_pRenderer = new JPH::DebugRendering::Renderer;
		m_pRenderer->Initialize();

		m_pFont = new JPH::Font(
			m_pRenderer,
			IO::Path::Combine(engine.GetInfo().GetDirectory(), MAKE_PATH("Code/DefaultPlugins/Physics/PhysicsCore/JoltDebugAssets"))
		);
		m_pFont->Create("Arial", 24);

		// Init debug renderer
		m_pDebugRenderer = new JPH::DebugRendering::DebugRendererImp(
			m_pRenderer,
			m_pFont,
			IO::Path::Combine(engine.GetInfo().GetDirectory(), MAKE_PATH("Code/DefaultPlugins/Physics/PhysicsCore/JoltDebugAssets"))
		);
#endif
	}

	void Plugin::OnUnloaded(Application&)
	{
		delete JPH::Factory::sInstance;
		JPH::Factory::sInstance = nullptr;
	}
}

#if PLUGINS_IN_EXECUTABLE
[[maybe_unused]] static bool entryPoint = ngine::Plugin::Register<ngine::Physics::Plugin>();
#else
extern "C" PHYSICS_EXPORT_API ngine::Plugin* InitializePlugin(ngine::Application& application)
{
	return new ngine::Physics::Plugin(application);
}
#endif
