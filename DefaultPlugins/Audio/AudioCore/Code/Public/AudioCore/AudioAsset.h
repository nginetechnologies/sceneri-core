#pragma once

#include "AudioAssetType.h"

#include <Common/Asset/Asset.h>
#include <Common/Asset/AssetFormat.h>
#include <Common/Asset/AssetTypeTag.h>
#include <Common/Reflection/Type.h>

namespace ngine::Asset
{
	struct Manager;
}

namespace ngine::Audio
{
	enum class PCMFormat : int16
	{
		U8,
		S8,
		Bit16,
		Bit24,
		Bit32,
		Bit64,
		Flt,
		Dbl,
		Invalid
	};

	[[nodiscard]] inline ConstStringView GetPCMFormatName(Audio::PCMFormat format)
	{
		switch (format)
		{
			case ngine::Audio::PCMFormat::U8:
				return "U8";
			case ngine::Audio::PCMFormat::S8:
				return "S8";
			case ngine::Audio::PCMFormat::Bit16:
				return "16";
			case ngine::Audio::PCMFormat::Bit24:
				return "24";
			case ngine::Audio::PCMFormat::Bit32:
				return "32";
			case ngine::Audio::PCMFormat::Bit64:
				return "64";
			case ngine::Audio::PCMFormat::Flt:
				return "Flt";
			case ngine::Audio::PCMFormat::Dbl:
				return "Dbl";
			case ngine::Audio::PCMFormat::Invalid:
				return "Invalid";
		}

		return {};
	}

	struct MetaData
	{
		int16 channelCount = 0;
		int32 sampleRate = 0;
		double lengthSeconds = 0.0;
		uint64 frameSize = 0; // channels * bits per sample
		PCMFormat sourceFormat = PCMFormat::Invalid;
	};

	struct FileHeader
	{
		static constexpr ConstStringView Marker = "AUDIO";
		char marker[6] = "AUDIO";
		uint16 version = 1;
		MetaData metaData;
		uint64 dataChunkSize = 0;
	};

	struct AudioAsset : public Asset::Asset
	{
		using Asset::Asset;

		AudioAsset(const Serialization::Data& assetData, IO::Path&& assetMetaFilePath);

		bool Serialize(const Serialization::Reader reader);
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Audio::AudioAsset>
	{
		inline static constexpr auto Type = Reflection::Reflect<Audio::AudioAsset>(
			Audio::AssetFormat.assetTypeGuid, MAKE_UNICODE_LITERAL("Audio"), Reflection::TypeFlags{}, Reflection::Tags{Asset::Tags::AssetType}
		);
	};
}
