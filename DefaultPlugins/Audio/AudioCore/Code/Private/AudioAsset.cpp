#include "AudioAsset.h"

#include <Engine/Asset/AssetManager.h>

#include <Common/Serialization/Deserialize.h>
#include <Common/Serialization/Reader.h>
#include <Common/Serialization/Writer.h>

namespace ngine::Audio
{
	bool AudioAsset::Serialize(const Serialization::Reader reader)
	{
		Asset::Asset::Serialize(reader);

		return true;
	}

	AudioAsset::AudioAsset(const Serialization::Data& assetData, IO::Path&& assetMetaFilePath)
	{
		m_metaDataFilePath = Forward<IO::Path>(assetMetaFilePath);
		Serialization::Deserialize(assetData, *this);
		SetTypeGuid(AssetFormat.assetTypeGuid);
	}
}
