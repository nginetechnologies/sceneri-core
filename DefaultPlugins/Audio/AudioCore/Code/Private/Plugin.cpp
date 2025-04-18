#include "Plugin.h"

#include "OpenAL/OpenALSystem.h"
#include "AudioAsset.h"
#include "VolumeType.h"

#include <Engine/Asset/AssetManager.h>
#include <Engine/Event/EventManager.h>

#include <Common/Reflection/Type.h>
#include <Common/Reflection/Registry.inl>
#include <Common/IO/Log.h>

#include <Common/System/Query.h>

#include <Engine/Input/DeviceType.h>

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Audio::Volume>
	{
		inline static constexpr auto Type = Reflection::Reflect<Audio::Volume>(Audio::Volume::TypeGuid, MAKE_UNICODE_LITERAL("Volume"));
	};
}

namespace ngine::Audio
{
	Plugin::Plugin(Application&)
		: m_cache(System::Get<Asset::Manager>())
	{
		GetInstance() = this;
	}

	void Plugin::OnLoaded(Application&)
	{
		// TODO: Workaround: On the web audio can only be initialized after the first click
#if !PLATFORM_WEB
		Initialize();
#else
		// Temp disabled until we figure out the audio playback issue
		Events::Manager& eventManager = System::Get<Events::Manager>();
		const Events::Identifier eventIdentifier = eventManager.FindOrRegisterEvent("7bc481f1-4c81-47bb-8782-f222cbf527f6"_guid);
		eventManager.Subscribe<&Plugin::Initialize>(eventIdentifier, *this);
#endif
	}

	void Plugin::OnUnloaded(Application&)
	{
		if (m_pSystem)
		{
			m_pSystem->Shutdown();
		}
	}

	void Plugin::Initialize()
	{
		if (m_isInitialized)
			return;

		m_pSystem = UniquePtr<OpenALSystem>::Make();
		InitializationResult result = m_pSystem->Initialize();
		if (UNLIKELY(result == InitializationResult::Failed))
		{
			LogError("Audio initialization failed!");
		}
		else
		{
			m_isInitialized = true;
		}
	}

	[[maybe_unused]] const bool wasAudioAssetTypeRegistered = Reflection::Registry::RegisterType<AudioAsset>();
	[[maybe_unused]] const bool wasVolumeTypeRegistered = Reflection::Registry::RegisterType<Volume>();
}

#if PLUGINS_IN_EXECUTABLE
[[maybe_unused]] static bool entryPoint = ngine::Plugin::Register<ngine::Audio::Plugin>();
#else
extern "C" AUDIO_EXPORT_API ngine::Plugin* InitializePlugin(ngine::Engine& engine)
{
	return new ngine::Audio::Plugin(engine);
}
#endif
