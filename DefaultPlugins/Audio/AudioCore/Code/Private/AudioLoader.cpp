#include "AudioLoader.h"

#include <Common/Memory/UniquePtr.h>
#include <Common/Memory/Copy.h>

#if HAS_LIBNYQUIST
#include <3rdparty/libnyquist/include/Decoders.h>
#endif

namespace ngine::Audio
{
#if HAS_LIBNYQUIST
	[[nodiscard]] inline PCMFormat ConvertFormat(nqr::PCMFormat nqrFormat)
	{
		switch (nqrFormat)
		{
			case nqr::PCM_U8:
			{
				return PCMFormat::U8;
			}
			case nqr::PCM_S8:
			{
				return PCMFormat::S8;
			}
			case nqr::PCM_16:
			{
				return PCMFormat::Bit16;
			}
			case nqr::PCM_24:
			{
				return PCMFormat::Bit24;
			}
			case nqr::PCM_32:
			{
				return PCMFormat::Bit32;
			}
			case nqr::PCM_64:
			{
				return PCMFormat::Bit64;
			}
			case nqr::PCM_DBL:
			{
				return PCMFormat::Dbl;
			}
			case nqr::PCM_FLT:
			{
				return PCMFormat::Flt;
			}
			case nqr::PCM_END:
			{
				return PCMFormat::Invalid;
			}
			default:
				ExpectUnreachable();
		}
	}
#endif

	Loader::Loader(IO::PathView sourceFile)
	{
#if HAS_LIBNYQUIST
		UniquePtr<nqr::AudioData> pAudioData = UniquePtr<nqr::AudioData>::Make();
		nqr::NyquistIO loader;

		String conversionString(sourceFile.GetStringView());
		std::string stdPath(conversionString.GetData(), conversionString.GetDataSize());
		loader.Load(pAudioData.Get(), stdPath);

		if (LIKELY(pAudioData->samples.size() != 0))
		{
			m_metaData.channelCount = (uint16)pAudioData->channelCount;
			m_metaData.frameSize = pAudioData->frameSize;
			m_metaData.lengthSeconds = pAudioData->lengthSeconds;
			m_metaData.sampleRate = pAudioData->sampleRate;
			m_metaData.sourceFormat = ConvertFormat(pAudioData->sourceFormat);

			const uint32 sampleDataSize = (uint32)pAudioData->samples.size();
			const uint32 samplesSizeInBytes = (sampleDataSize * GetFormatBitsPerSample(pAudioData->sourceFormat)) / 8;
			m_audioData.Resize(samplesSizeInBytes);
			nqr::ConvertFromFloat32(m_audioData.GetData(), pAudioData->samples.data(), sampleDataSize, pAudioData->sourceFormat);
		}
#else
		UNUSED(sourceFile);
#endif
	}

	const MetaData& Loader::GetMetaData() const
	{
		return m_metaData;
	}

	const Loader::AudioData& Loader::GetAudioData() const
	{
		return m_audioData;
	}

	[[nodiscard]] inline bool IsChannelSupported(uint16 channelCount)
	{
		// For now we only support mono audio because OpenAL will only play mono audio in the 3D world
		// Later we should introduce the concept of music which can support stereo
		return channelCount == 1;
	}

	[[nodiscard]] inline bool IsPCMFormatSupported(Audio::PCMFormat format)
	{
		// OpenAL only supports either 8 or 16 bit format at the moment.
		return format == Audio::PCMFormat::U8 || format == Audio::PCMFormat::Bit16;
	}

	bool Loader::IsFormatSupported() const
	{
		if (!IsChannelSupported(m_metaData.channelCount))
		{
			return false;
		}

		if (!IsPCMFormatSupported(m_metaData.sourceFormat))
		{
			return false;
		}

		return true;
	}

	bool Loader::TryConvertToSupported()
	{
		if (IsFormatSupported())
		{
			return true;
		}

		if (!IsPCMFormatSupported(m_metaData.sourceFormat))
		{
			switch (m_metaData.sourceFormat)
			{
				case PCMFormat::U8:
				case PCMFormat::S8:
				case PCMFormat::Bit16:
				{
				}
				break;
				case PCMFormat::Bit24:
				{
					const uint32 payloadSize = m_audioData.GetSize();
					uint8_t* first = m_audioData.GetData();
					uint32 counter = 0;
					for (uint32 i = 0; i < payloadSize; i += 3)
					{
						first[counter] = first[i + 1];
						first[counter + 1] = first[i + 2];
						counter += 2;
					}

					const uint32 size = payloadSize - payloadSize / 3;
					m_audioData.Resize(size);

					m_metaData.frameSize = sizeof(int16) * m_metaData.channelCount;
					m_metaData.sourceFormat = Audio::PCMFormat::Bit16;
				}
				break;
				case PCMFormat::Bit32:
				{
					return false;
				}
				case PCMFormat::Bit64:
				{
					return false;
				}
				case PCMFormat::Flt:
				{
					return false;
				}
				case PCMFormat::Dbl:
				{
					return false;
				}
				case PCMFormat::Invalid:
				{
					return false;
				}
			}
		}

		if (!IsChannelSupported(m_metaData.channelCount))
		{
			if (m_metaData.channelCount == 2)
			{
				switch (m_metaData.sourceFormat)
				{
					case PCMFormat::U8:
						CombineChannels<uint8>();
						break;
					case PCMFormat::S8:
						CombineChannels<int8>();
						break;
					case PCMFormat::Bit16:
						CombineChannels<int16>();
						break;
					case PCMFormat::Bit32:
						CombineChannels<int32>();
						break;
					case PCMFormat::Bit64:
						CombineChannels<int64>();
						break;
						// Types are not supported yet.
					case PCMFormat::Bit24:
					case PCMFormat::Flt:
					case PCMFormat::Dbl:
					case PCMFormat::Invalid:
						return false;
				}

				return true;
			}
			else
			{
				return false;
			}
		}

		return true;
	}

}
