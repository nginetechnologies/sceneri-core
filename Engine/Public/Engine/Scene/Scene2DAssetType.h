#pragma once

#include <Common/Asset/AssetFormat.h>
#include <Common/Asset/AssetTypeTag.h>
#include <Common/Reflection/Type.h>

namespace ngine
{
	struct Scene2DAssetType
	{
		inline static constexpr ngine::Asset::Format AssetFormat = {
			"7e77c698-27c3-4e41-a062-31dbe4f34596"_guid, MAKE_PATH(".scene2D.nasset"), {}, ngine::Asset::Format::Flags::IsCollection
		};
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Scene2DAssetType>
	{
		inline static constexpr auto Type = Reflection::Reflect<Scene2DAssetType>(
			Scene2DAssetType::AssetFormat.assetTypeGuid,
			MAKE_UNICODE_LITERAL("2D Scene"),
			Reflection::TypeFlags{},
			Reflection::Tags{Asset::Tags::AssetType}
		);
	};
}
