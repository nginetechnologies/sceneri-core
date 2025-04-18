#include <Renderer/Assets/Texture/TextureAsset.h>
#include <Renderer/Assets/Texture/TextureAssetType.h>

#include <Common/Serialization/Deserialize.h>
#include <Common/Serialization/Writer.h>
#include <Common/Memory/Containers/Serialization/Vector.h>
#include <Common/Serialization/Guid.h>

namespace ngine::Rendering
{
	TextureAsset::TextureAsset(const Serialization::Data& assetData, IO::Path&& assetMetaFilePath)
	{
		m_metaDataFilePath = Forward<IO::Path>(assetMetaFilePath);
		Serialization::Deserialize(assetData, *this);
		SetTypeGuid(TextureAssetType::AssetFormat.assetTypeGuid);
	}

	bool TextureAsset::Serialize(const Serialization::Reader serializer)
	{
		Asset::Asset::Serialize(serializer);
		serializer.Serialize("resolution", m_resolution);
		serializer.Serialize("preset", m_preset);
		if (!serializer.Serialize("usage_flags", m_usageFlags))
		{
			m_usageFlags = UsageFlags::Sampled | UsageFlags::TransferDestination;
		}
		serializer.Serialize("flags", m_flags);
		if (!serializer.Serialize("array_size", m_arraySize))
		{
			m_arraySize = 1;
		}

		serializer.Serialize("bc_data", GetBinaryAssetInfo(BinaryType::BC));
		serializer.Serialize("astc_data", GetBinaryAssetInfo(BinaryType::ASTC));
		if (!serializer.Serialize("generate_mips", m_generateMips))
		{
			m_generateMips = true;
		}

		return true;
	}

	bool TextureAsset::Serialize(Serialization::Writer serializer) const
	{
		Asset::Asset::Serialize(serializer);

		serializer.Serialize("resolution", m_resolution);
		serializer.Serialize("preset", m_preset);
		serializer.Serialize("usage_flags", m_usageFlags);
		serializer.Serialize("flags", m_flags);
		if (m_arraySize > 1)
		{
			serializer.Serialize("array_size", m_arraySize);
		}

		serializer.Serialize("bc_data", GetBinaryAssetInfo(BinaryType::BC));
		serializer.Serialize("astc_data", GetBinaryAssetInfo(BinaryType::ASTC));
		serializer.SerializeWithDefaultValue("generate_mips", m_generateMips, true);

		return true;
	}

	bool TextureAsset::BinaryInfo::Serialize(const Serialization::Reader serializer)
	{
		serializer.Serialize("format", m_format);
		m_mipInfo.Clear();
		serializer.Serialize("mipmap_info", m_mipInfo);
		m_compressionQuality = serializer.Read<Math::Ratiof>("compression_quality");
		return true;
	}

	bool TextureAsset::BinaryInfo::Serialize(Serialization::Writer serializer) const
	{
		if (m_format != Rendering::Format::Invalid)
		{
			serializer.Serialize("format", m_format);
		}
		serializer.Serialize("mipmap_info", m_mipInfo);
		if (m_compressionQuality.IsValid())
		{
			serializer.Serialize("compression_quality", *m_compressionQuality);
		}
		return true;
	}

	bool TextureAsset::MipInfo::Serialize(const Serialization::Reader serializer)
	{
		serializer.Serialize("level", level);
		serializer.Serialize("offset", m_offset);
		serializer.Serialize("size", m_size);
		return true;
	}

	bool TextureAsset::MipInfo::Serialize(Serialization::Writer serializer) const
	{
		serializer.Serialize("level", level);
		serializer.Serialize("offset", m_offset);
		serializer.Serialize("size", m_size);
		return true;
	}

	IO::Path TextureAsset::GetBinaryFilePath(const BinaryType type) const
	{
		return Asset::GetBinaryFilePath(GetBinaryFileExtension(type));
	}
}
