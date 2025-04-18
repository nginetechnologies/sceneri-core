#pragma once

#include "VolumeType.h"

#include <Common/Math/ForwardDeclarations/Vector3.h>

namespace ngine::Audio
{
	struct Listener
	{
		Listener() = default;

		virtual ~Listener()
		{
		}

		Listener(const Listener&) = delete;
		Listener& operator=(const Listener&) = delete;

		virtual void SetPosition(Math::Vector3f position) = 0;
		virtual void SetVolume(Volume volume) = 0;
		virtual void SetVelocity(Math::Vector3f velocity) = 0;
		virtual void SetOrientation(Math::Vector3f up, Math::Vector3f forward) = 0;
	};
}
