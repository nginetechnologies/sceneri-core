#pragma once

#include "TextureAsset.h"

#include <Renderer/ImageAspectFlags.h>
#include <Renderer/Wrappers/ImageFlags.h>
#include <Renderer/FormatInfo.h>

namespace ngine::Rendering
{
	struct RenderTargetAsset : public TextureAsset
	{
		inline static constexpr ngine::Asset::Format AssetFormat = {
			"{7D2D2421-A875-450F-AEDF-15C4C5C91F31}"_guid, MAKE_PATH(".rendertarget.nasset")
		};

		RenderTargetAsset() = default;
		RenderTargetAsset(const Serialization::Data& assetData, IO::Path&& assetMetaFilePath);

		bool Serialize(const Serialization::Reader serializer);
		bool Serialize(Serialization::Writer serializer) const;

		void SetBinaryAssetInfo(const BinaryType type, BinaryInfo&& info)
		{
			m_binaryInfo[Math::Log2((uint8)type)] = Forward<BinaryInfo>(info);
		}

		[[nodiscard]] EnumFlags<ImageFlags> GetImageFlags() const
		{
			return m_imageFlags;
		}
	protected:
		Rendering::Format m_format = Rendering::Format::Invalid;
		EnumFlags<ImageFlags> m_imageFlags;
	};
}
