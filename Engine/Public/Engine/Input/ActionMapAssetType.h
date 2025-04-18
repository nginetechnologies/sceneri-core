
#pragma once

#include <Common/Asset/AssetFormat.h>
#include <Common/Asset/AssetTypeTag.h>
#include <Common/Reflection/Type.h>

namespace ngine::Input
{
	struct ActionMapAssetType
	{
		inline static constexpr ngine::Asset::Format AssetFormat = {
			"{6FF5D6E8-62AB-4C26-AA4B-05765922A68A}"_guid, MAKE_PATH(".actionmap.nasset"), {}, {}
		};
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Input::ActionMapAssetType>
	{
		inline static constexpr auto Type = Reflection::Reflect<Input::ActionMapAssetType>(
			Input::ActionMapAssetType::AssetFormat.assetTypeGuid,
			MAKE_UNICODE_LITERAL("ActionMap"),
			Reflection::TypeFlags{},
			Reflection::Tags{Asset::Tags::AssetType}
		);
	};
}
