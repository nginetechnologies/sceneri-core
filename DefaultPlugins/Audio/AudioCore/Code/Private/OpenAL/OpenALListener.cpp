#include "OpenAL/OpenALListener.h"

#if PLATFORM_WEB
#include <AL/al.h>
#include <AL/alc.h>
#else
#include <3rdparty/openal-soft/include/AL/al.h>
#include <3rdparty/openal-soft/include/AL/alc.h>
#endif

#include <Common/Math/Vector3.h>

namespace ngine::Audio
{
	OpenALListener::~OpenALListener()
	{
	}

	void OpenALListener::SetPosition(Math::Vector3f position)
	{
		alListener3f(AL_POSITION, position.x, position.y, position.z);
	}

	void OpenALListener::SetVolume(Volume volume)
	{
		alListenerf(AL_GAIN, volume);
	}

	void OpenALListener::SetVelocity(Math::Vector3f velocity)
	{
		alListener3f(AL_VELOCITY, velocity.x, velocity.y, velocity.z);
	}

	void OpenALListener::SetOrientation(Math::Vector3f up, Math::Vector3f forward)
	{
		ALfloat positionData[6];
		positionData[0] = forward.x;
		positionData[1] = forward.y;
		positionData[2] = forward.z;
		positionData[3] = up.x;
		positionData[4] = up.y;
		positionData[5] = up.z;
		alListenerfv(AL_ORIENTATION, &positionData[0]);
	}
}
