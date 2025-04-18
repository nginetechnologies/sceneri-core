#pragma once

#include <Engine/Asset/AssetTypeEditorExtension.h>

#include <Common/Asset/AssetFormat.h>
#include <Common/Asset/AssetTypeTag.h>
#include <Common/Reflection/Type.h>

namespace ngine::Physics
{
	struct MaterialAssetType
	{
		inline static constexpr ngine::Asset::Format AssetFormat = {
			"{DDA212F1-B3E6-406B-B801-2079D4A65561}"_guid, MAKE_PATH(".physmat.nasset")
		};
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Physics::MaterialAssetType>
	{
		inline static constexpr auto Type = Reflection::Reflect<Physics::MaterialAssetType>(
			Physics::MaterialAssetType::AssetFormat.assetTypeGuid,
			MAKE_UNICODE_LITERAL("Physical Material"),
			Reflection::TypeFlags{},
			Reflection::Tags{Asset::Tags::AssetType},
			Properties{},
			Functions{},
			Events{},
			Extensions{Asset::EditorTypeExtension{
				"6148a6e9-a8a7-41dd-8b1b-b9e8757a2a7f"_guid,
				"d9eff402-c798-4689-b546-aa4ffc4752da"_asset,
				"26a5b8ed-3efd-4fa8-804d-aa35b847d18f"_asset,
				"a9d18d3e-09fe-458c-9816-ba51ca957b6a"_asset
			}}
		);
	};
}
