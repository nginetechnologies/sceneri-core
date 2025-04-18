#pragma once

#include <Engine/Asset/AssetTypeEditorExtension.h>

#include <Common/Asset/AssetFormat.h>
#include <Common/Asset/AssetTypeTag.h>
#include <Common/Reflection/Type.h>

namespace ngine
{
	struct MaterialInstanceAssetType
	{
		inline static constexpr ngine::Asset::Format AssetFormat = {"CE3B6C53-1B6D-4BC6-97E6-2EA036C58C0F"_guid, MAKE_PATH(".mtli.nasset")};
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<MaterialInstanceAssetType>
	{
		inline static constexpr auto Type = Reflection::Reflect<MaterialInstanceAssetType>(
			MaterialInstanceAssetType::AssetFormat.assetTypeGuid,
			MAKE_UNICODE_LITERAL("Material"),
			Reflection::TypeFlags{},
			Reflection::Tags{Asset::Tags::AssetType},
			Properties{},
			Functions{},
			Events{},
			Extensions{Asset::EditorTypeExtension{
				"748baa5c-f91d-4e56-9257-28a76a3c15cc"_guid,
				"2e0f0318-3440-4654-b859-02fc7b881eb8"_asset,
				"ae361bd2-f710-4fb4-8ff6-1651dbc022fb"_asset,
				"a9d18d3e-09fe-458c-9816-ba51ca957b6a"_asset
			}}
		);
	};
}
