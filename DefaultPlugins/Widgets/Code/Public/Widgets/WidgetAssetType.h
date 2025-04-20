#pragma once

#include <Common/Asset/AssetFormat.h>
#include <Common/Asset/AssetTypeTag.h>
#include <Common/Reflection/Type.h>

namespace ngine
{
	struct WidgetAssetType
	{
		inline static constexpr ngine::Asset::Format AssetFormat = {"ded54a50-9829-41c5-a9b7-189387705aa7"_guid, MAKE_PATH(".widget.nasset")};
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<WidgetAssetType>
	{
		inline static constexpr auto Type = Reflection::Reflect<WidgetAssetType>(
			WidgetAssetType::AssetFormat.assetTypeGuid,
			MAKE_UNICODE_LITERAL("Widget Asset Type"),
			Reflection::TypeFlags{},
			Reflection::Tags{Asset::Tags::AssetType}
		);
	};
}
