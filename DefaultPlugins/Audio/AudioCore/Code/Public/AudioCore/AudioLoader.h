#pragma once

#include "AudioAsset.h"

#include <Common/IO/Path.h>
#include <Common/Memory/Containers/Vector.h>

namespace ngine::Audio
{
	struct Loader
	{
		explicit Loader(IO::PathView sourcePath);
		Loader(const Loader&) = delete;
		Loader& operator=(const Loader&) = delete;
		Loader(const Loader&&) = delete;
		Loader&& operator=(const Loader&&) = delete;

		[[nodiscard]] const MetaData& GetMetaData() const;
		using AudioData = Vector<ByteType>;
		[[nodiscard]] const AudioData& GetAudioData() const;

		[[nodiscard]] bool IsValid() const
		{
			return !m_audioData.IsEmpty();
		}

		[[nodiscard]] bool IsFormatSupported() const;

		[[nodiscard]] bool TryConvertToSupported();

		template<typename Type>
		void CombineChannels()
		{
			const uint32 frameSizeMono = (uint32)(m_metaData.frameSize / m_metaData.channelCount);
			ByteType* pAudio = m_audioData.GetData();

			ByteType* pWriter = pAudio;
			ByteType* pChannel1 = pAudio;
			ByteType* pChannel2 = pAudio + frameSizeMono;

			const uint32 totalSampleCount = uint32(m_audioData.GetSize() / m_metaData.frameSize);
			uint32 sampleCount = 0;
			while (sampleCount <= totalSampleCount)
			{
				Type value = (*reinterpret_cast<Type*>(pChannel1) / 2) + (*reinterpret_cast<Type*>(pChannel2) / 2);
				Memory::CopyWithoutOverlap(pWriter, &value, sizeof(Type));

				pChannel1 += m_metaData.frameSize;
				pChannel2 = pChannel1 + frameSizeMono;
				pWriter += frameSizeMono;

				sampleCount++;
			}

			m_metaData.channelCount = 1;
			m_metaData.frameSize = frameSizeMono;

			m_audioData.Resize(totalSampleCount * frameSizeMono);
		}
	private:
		MetaData m_metaData;
		AudioData m_audioData;
	};
}
