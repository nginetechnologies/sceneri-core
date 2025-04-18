#pragma once

#include <Common/Asset/AssetFormat.h>
#include <Common/Asset/AssetTypeTag.h>
#include <Common/Reflection/Type.h>

namespace ngine
{
	struct MaterialAssetType
	{
		inline static constexpr ngine::Asset::Format AssetFormat = {"{6717FF80-6896-4396-A66C-B493E5FE634C}"_guid, MAKE_PATH(".mtl.nasset")};
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<MaterialAssetType>
	{
		inline static constexpr auto Type = Reflection::Reflect<MaterialAssetType>(
			MaterialAssetType::AssetFormat.assetTypeGuid,
			MAKE_UNICODE_LITERAL("Material Template"),
			Reflection::TypeFlags{},
			Reflection::Tags{Asset::Tags::AssetType}
		);
	};
}
