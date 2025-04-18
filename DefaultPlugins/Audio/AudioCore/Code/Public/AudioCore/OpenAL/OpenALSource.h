#pragma once

#include "AudioSource.h"

#if PLATFORM_WEB
#include <AL/al.h>
#else
#include <3rdparty/openal-soft/include/AL/al.h>
#endif

namespace ngine::Audio
{
	struct OpenALSource final : Audio::Source
	{
		OpenALSource(const Audio::Data& data);

		virtual ~OpenALSource();

		OpenALSource(const OpenALSource&) = delete;
		OpenALSource& operator=(const OpenALSource&) = delete;

		// Audio::Source
		virtual void Play() override;
		virtual void Pause() override;
		virtual void Stop() override;
		[[nodiscard]] virtual bool IsPlaying() const override;

		virtual void SetLooping(bool shouldLoop) override;
		virtual void SetVolume(Volume volume) override;
		virtual void SetMaximumDistance(const Math::Lengthf distance) override;
		virtual void SetReferenceDistance(const Math::Lengthf distance) override;
		virtual void SetPosition(Math::Vector3f position) override;

		[[nodiscard]] virtual bool IsValid() const override;
		// ~Audio::Source
	private:
		ALuint m_source;
	};
}
