#include <Renderer/Assets/Material/MaterialInstanceAsset.h>
#include <Renderer/Assets/Material/MaterialInstanceAssetType.h>

#include <Common/Serialization/Deserialize.h>
#include <Common/Serialization/Serialize.h>
#include <Common/Serialization/Guid.h>
#include <Common/Memory/Containers/Serialization/Vector.h>

namespace ngine::Rendering
{
	MaterialInstanceAsset::MaterialInstanceAsset(Serialization::Data& assetData, IO::Path&& assetMetaFilePath)
	{
		m_metaDataFilePath = Forward<IO::Path>(assetMetaFilePath);
		Serialization::Deserialize(assetData, *this);
		SetTypeGuid(MaterialInstanceAssetType::AssetFormat.assetTypeGuid);
	}

	bool MaterialInstanceAsset::DescriptorContent::Serialize(const Serialization::Reader serializer)
	{
		if (Optional<Asset::Guid> textureGuid = serializer.Read<Asset::Guid>("texture"))
		{
			m_type = Type::Texture;
			m_textureInfo.m_assetGuid = textureGuid.Get();
			if (!serializer.Serialize("address_mode", m_textureInfo.m_addressMode))
			{
				m_textureInfo.m_addressMode = Rendering::AddressMode::Repeat;
			}
		}

		return true;
	}

	bool MaterialInstanceAsset::DescriptorContent::Serialize(Serialization::Writer serializer) const
	{
		switch (m_type)
		{
			case Type::Invalid:
				return false;
			case Type::Texture:
			{
				serializer.Serialize("texture", m_textureInfo.m_assetGuid);
				serializer.SerializeWithDefaultValue("address_mode", m_textureInfo.m_addressMode, Rendering::AddressMode::Repeat);
				return true;
			}
		}
		ExpectUnreachable();
	}

	bool MaterialInstanceAsset::Serialize(const Serialization::Reader serializer)
	{
		Asset::Serialize(serializer);
		serializer.Serialize("material", m_materialAssetGuid);
		serializer.Serialize("descriptor_contents", m_descriptorContents);
		serializer.Serialize("push_constants", m_pushConstants);

		return true;
	}

	bool MaterialInstanceAsset::Serialize(Serialization::Writer serializer) const
	{
		Assert(m_typeGuid == MaterialInstanceAssetType::AssetFormat.assetTypeGuid);
		Asset::Serialize(serializer);
		serializer.Serialize("material", m_materialAssetGuid);
		serializer.Serialize("descriptor_contents", m_descriptorContents);
		serializer.Serialize("push_constants", m_pushConstants);

		return true;
	}
}
