#include <Renderer/Assets/Texture/RenderTargetAsset.h>

#include <Common/Serialization/Deserialize.h>

namespace ngine::Rendering
{
	RenderTargetAsset::RenderTargetAsset(const Serialization::Data& assetData, IO::Path&& assetMetaFilePath)
	{
		m_metaDataFilePath = Forward<IO::Path>(assetMetaFilePath);
		Serialization::Deserialize(assetData, *this);
		SetTypeGuid(AssetFormat.assetTypeGuid);
	}

	bool RenderTargetAsset::Serialize(const Serialization::Reader serializer)
	{
		bool result = TextureAsset::Serialize(serializer);
		serializer.Serialize("resolution", m_resolution);
		serializer.Serialize("image_flags", m_imageFlags);
		return result;
	}

	bool RenderTargetAsset::Serialize(Serialization::Writer serializer) const
	{
		bool result = TextureAsset::Serialize(serializer);
		serializer.Serialize("resolution", m_resolution);
		serializer.Serialize("image_flags", m_imageFlags);
		return result;
	}
}
