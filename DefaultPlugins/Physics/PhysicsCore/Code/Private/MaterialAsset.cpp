#include "MaterialAsset.h"
#include "MaterialAssetType.h"

#include <Engine/Asset/AssetManager.h>

#include <Common/Serialization/Deserialize.h>
#include <Common/Reflection/Registry.inl>

namespace ngine::Physics
{
	bool MaterialAsset::Serialize(const Serialization::Reader reader)
	{
		reader.Serialize("friction", m_friction);
		reader.Serialize("restitution", m_restitution);
		reader.Serialize("density", m_density);
		return true;
	}

	MaterialAsset::MaterialAsset(const Serialization::Data& assetData, IO::Path&& assetMetaFilePath)
	{
		m_metaDataFilePath = Forward<IO::Path>(assetMetaFilePath);
		Serialization::Deserialize(assetData, *this);
		SetTypeGuid(MaterialAssetType::AssetFormat.assetTypeGuid);
	}

	[[maybe_unused]] const bool wasMaterialAssetTypeRegistered = Reflection::Registry::RegisterType<MaterialAssetType>();
}
