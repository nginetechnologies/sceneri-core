#pragma once

#include <Common/Asset/AssetFormat.h>
#include <Common/Asset/AssetTypeTag.h>
#include <Common/Reflection/Type.h>

namespace ngine
{
	struct FontAssetType
	{
		inline static constexpr ngine::Asset::Format AssetFormat = {"f5b9917c-7a80-4535-b14f-b94ae1621cd5"_guid, MAKE_PATH(".font.nasset")};
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<FontAssetType>
	{
		inline static constexpr auto Type = Reflection::Reflect<FontAssetType>(
			FontAssetType::AssetFormat.assetTypeGuid,
			MAKE_UNICODE_LITERAL("Font Asset Type"),
			Reflection::TypeFlags{},
			Reflection::Tags{Asset::Tags::AssetType}
		);
	};
}
