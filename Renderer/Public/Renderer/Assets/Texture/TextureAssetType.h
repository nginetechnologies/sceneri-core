#pragma once

#include <Common/Asset/AssetFormat.h>
#include <Common/Asset/AssetTypeTag.h>
#include <Common/Reflection/Type.h>

namespace ngine
{
	struct TextureAssetType
	{
		inline static constexpr ngine::Asset::Format AssetFormat = {
			"{C4E01AA2-495D-4EB1-B9BE-FE0E30500B07}"_guid, MAKE_PATH(".tex.nasset"), MAKE_PATH(".tex")
		};
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<TextureAssetType>
	{
		inline static constexpr auto Type = Reflection::Reflect<TextureAssetType>(
			TextureAssetType::AssetFormat.assetTypeGuid,
			MAKE_UNICODE_LITERAL("Texture Asset Type"),
			Reflection::TypeFlags{},
			Reflection::Tags{Asset::Tags::AssetType}
		);
	};
}
