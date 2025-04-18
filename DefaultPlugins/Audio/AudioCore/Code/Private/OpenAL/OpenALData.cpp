#include "OpenAL/OpenALData.h"
#include "AudioAsset.h"

#include <Common/Memory/UniquePtr.h>
#include <Common/IO/Log.h>

namespace ngine::Audio
{
	OpenALData::~OpenALData()
	{
		if (IsValid())
		{
			alDeleteBuffers(1, &m_buffer);
		}
	}

	[[nodiscard]] static ALenum ConvertAudioFormat(const MetaData& data)
	{
		if (data.channelCount == 1)
		{
			switch (data.sourceFormat)
			{
				case PCMFormat::U8:
				{
					return AL_FORMAT_MONO8;
				}
				case PCMFormat::Bit16:
				{
					return AL_FORMAT_MONO16;
				}
				default:
				{
					// Format not supported
					return 0;
				}
			}
		}
		else if (data.channelCount == 2)
		{
			switch (data.sourceFormat)
			{
				case PCMFormat::U8:
				{
					return AL_FORMAT_STEREO8;
				}

				case PCMFormat::Bit16:
				{
					return AL_FORMAT_STEREO16;
				}

				default:
				{
					// Format not supported
					return 0;
				}
			}
		}

		return 0;
	}

	bool OpenALData::Load(const ConstByteView data)
	{
		if (data.GetDataSize() < sizeof(FileHeader))
		{
			LogError("Audio data was smaller than header");
			return false;
		}

		const FileHeader* pHeader = reinterpret_cast<const FileHeader*>(data.GetData());
		if (FileHeader::Marker != pHeader->marker)
		{
			LogError("Could not read audio data!");
			return false;
		}

		if (data.GetDataSize() < sizeof(FileHeader) + pHeader->dataChunkSize)
		{
			LogError("Audio data size mismatch");
			return false;
		}

		ALuint buffer = 0;
		ALenum error;
		ALenum format = ConvertAudioFormat(pHeader->metaData);
		ALsizei size = (ALsizei)pHeader->dataChunkSize;

		const ByteType* pRawAudioData = data.GetData() + sizeof(FileHeader);

		alGenBuffers(1, &buffer);
		alBufferData(buffer, format, pRawAudioData, size, pHeader->metaData.sampleRate);

		error = alGetError();
		if (UNLIKELY(error != AL_NO_ERROR))
		{
			const char* errorString = alGetString(error);
			if (errorString != nullptr)
			{
				LogError("OpenAL error: {}", errorString);
			}
			else
			{

				LogError("Unknown OpenAL error");
			}
			if (buffer && alIsBuffer(buffer))
				alDeleteBuffers(1, &buffer);
			return false;
		}

		m_buffer = buffer;

		return true;
	}

	bool OpenALData::IsLoaded() const
	{
		return m_buffer != 0;
	}

	bool OpenALData::IsValid() const
	{
		return m_buffer != 0 && alIsBuffer(m_buffer);
	}

	ALuint OpenALData::GetALBuffer() const
	{
		return m_buffer;
	}
}
