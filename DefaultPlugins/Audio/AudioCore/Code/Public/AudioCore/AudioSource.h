#pragma once

#include "VolumeType.h"
#include "AudioData.h"

#include <Common/Math/ForwardDeclarations/Length.h>
#include <Common/Math/ForwardDeclarations/Vector3.h>

namespace ngine::Audio
{
	struct Source
	{
		Source(const Audio::Data& data)
			: m_data(data)
		{
		}

		virtual ~Source()
		{
		}

		Source(const Source&) = delete;
		Source& operator=(const Source&) = delete;

		virtual void Play() = 0;
		virtual void Pause() = 0;
		virtual void Stop() = 0;
		[[nodiscard]] virtual bool IsPlaying() const = 0;

		virtual void SetLooping(bool shouldLoop) = 0;
		virtual void SetVolume(Volume volume) = 0;
		virtual void SetMaximumDistance(const Math::Lengthf distance) = 0;
		virtual void SetReferenceDistance(const Math::Lengthf distance) = 0;
		virtual void SetPosition(Math::Vector3f position) = 0;

		[[nodiscard]] virtual bool IsValid() const = 0;
	protected:
		const Audio::Data& m_data;
	};
}
