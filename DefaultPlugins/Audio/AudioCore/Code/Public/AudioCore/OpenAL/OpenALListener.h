#pragma once

#include "AudioListener.h"

#if PLATFORM_WEB
#include <AL/al.h>
#else
#include <3rdparty/openal-soft/include/AL/al.h>
#endif

namespace ngine::Audio
{
	struct OpenALListener final : Audio::Listener
	{
		OpenALListener() = default;

		virtual ~OpenALListener();

		OpenALListener(const OpenALListener&) = delete;
		OpenALListener& operator=(const OpenALListener&) = delete;

		// Audio::Listener
		virtual void SetPosition(Math::Vector3f position) override;
		virtual void SetVolume(Volume volume) override;
		virtual void SetVelocity(Math::Vector3f velocity) override;
		virtual void SetOrientation(Math::Vector3f up, Math::Vector3f forward) override;
		// ~Audio::Listener
	private:
	};
}
