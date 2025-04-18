#pragma once

#include <Common/Asset/AssetFormat.h>
#include <Common/Asset/AssetTypeTag.h>
#include <Common/Reflection/Type.h>

namespace ngine
{
	struct MeshSceneAssetType
	{
		inline static constexpr Asset::Format AssetFormat = {
			"{ABCF5A98-C556-41BB-B7AC-A5C6EA9DD85C}"_guid, MAKE_PATH(".mesh.nasset"), MAKE_PATH(".mesh"), Asset::Format::Flags::IsCollection
		};
	};

	struct MeshPartAssetType
	{
		inline static constexpr Asset::Format AssetFormat = {
			"3418a661-1034-40c4-9e40-04221b1177df"_guid, MAKE_PATH(".mesh.nasset"), MAKE_PATH(".mesh"), Asset::Format::Flags{}
		};
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<MeshSceneAssetType>
	{
		inline static constexpr auto Type = Reflection::Reflect<MeshSceneAssetType>(
			MeshSceneAssetType::AssetFormat.assetTypeGuid,
			MAKE_UNICODE_LITERAL("Mesh"),
			Reflection::TypeFlags{},
			Reflection::Tags{Asset::Tags::AssetType}
		);
	};

	template<>
	struct ReflectedType<MeshPartAssetType>
	{
		inline static constexpr auto Type = Reflection::Reflect<MeshPartAssetType>(
			MeshPartAssetType::AssetFormat.assetTypeGuid,
			MAKE_UNICODE_LITERAL("Mesh Part"),
			Reflection::TypeFlags{},
			Reflection::Tags{Asset::Tags::AssetType}
		);
	};
}
