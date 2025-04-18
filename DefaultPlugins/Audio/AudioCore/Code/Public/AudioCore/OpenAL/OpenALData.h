#pragma once

#include "AudioData.h"

#if PLATFORM_WEB
#include <AL/al.h>
#else
#include <3rdparty/openal-soft/include/AL/al.h>
#endif

namespace ngine::Audio
{
	struct OpenALData final : Audio::Data
	{
		using BaseType = Audio::Data;
		using BaseType::BaseType;

		virtual ~OpenALData();

		OpenALData(const OpenALData&) = delete;
		OpenALData& operator=(const OpenALData&) = delete;

		// Audio::Data
		virtual bool Load(const ConstByteView data) override;

		[[nodiscard]] virtual bool IsLoaded() const override;
		[[nodiscard]] virtual bool IsValid() const override;
		// ~Audio::Data

		[[nodiscard]] ALuint GetALBuffer() const;
	private:
		ALuint m_buffer;
	};
}
