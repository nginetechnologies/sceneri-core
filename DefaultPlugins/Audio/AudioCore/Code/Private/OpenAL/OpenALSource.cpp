#include "OpenAL/OpenALSource.h"
#include "OpenAL/OpenALData.h"

#include <Common/Math/Vector3.h>
#include <Common/Math/Length.h>
#include <Common/IO/Log.h>

namespace ngine::Audio
{
	OpenALSource::OpenALSource(const Audio::Data& data)
		: Audio::Source(data)
	{
		const OpenALData& alData = static_cast<const OpenALData&>(data);

		alGenSources(1, &m_source);
		alSourcei(m_source, AL_BUFFER, alData.GetALBuffer());

		LogErrorIf(alGetError() != AL_NO_ERROR, "Failed to setup sound source");
	}

	OpenALSource::~OpenALSource()
	{
		if (UNLIKELY(!IsValid()))
			return;

		alDeleteSources(1, &m_source);
	}

	void OpenALSource::Play()
	{
		if (UNLIKELY(!IsValid()))
			return;

		alSourcePlay(m_source);
	}

	void OpenALSource::Pause()
	{
		if (UNLIKELY(!IsValid()))
			return;

		alSourcePause(m_source);
	}

	void OpenALSource::Stop()
	{
		if (UNLIKELY(!IsValid()))
			return;

		alSourceStop(m_source);
	}

	bool OpenALSource::IsPlaying() const
	{
		if (UNLIKELY(!IsValid()))
			return false;

		ALint value{0};
		alGetSourcei(m_source, AL_PLAYING, &value);
		return value != 0;
	}

	void OpenALSource::SetLooping(bool shouldLoop)
	{
		if (UNLIKELY(!IsValid()))
			return;

		alSourcei(m_source, AL_LOOPING, shouldLoop ? 1 : 0);
	}

	void OpenALSource::SetVolume(Volume volume)
	{
		if (UNLIKELY(!IsValid()))
			return;

		alSourcef(m_source, AL_GAIN, volume);
	}

	void OpenALSource::SetPosition(const Math::Vector3f position)
	{
		alSource3f(m_source, AL_POSITION, position.x, position.y, position.z);
	}

	void OpenALSource::SetMaximumDistance(const Math::Lengthf distance)
	{
		alSourcef(m_source, AL_MAX_DISTANCE, distance.GetMeters());
	}

	void OpenALSource::SetReferenceDistance(const Math::Lengthf distance)
	{
		alSourcef(m_source, AL_REFERENCE_DISTANCE, distance.GetMeters());
	}

	bool OpenALSource::IsValid() const
	{
		return m_source != 0 && alIsSource(m_source);
	}
}
