#pragma once

#include <Common/Asset/AssetFormat.h>
#include <Common/Asset/AssetTypeTag.h>
#include <Common/Reflection/Type.h>

namespace ngine::Animation
{
	struct MeshSkinAssetType
	{
		inline static constexpr Asset::Format AssetFormat = {
			"{3F5AB03E-BCA9-4A4A-A864-E539033AD66C}"_guid, MAKE_PATH(".meshskin.nasset"), MAKE_PATH(".meshskin")
		};
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Animation::MeshSkinAssetType>
	{
		inline static constexpr auto Type = Reflection::Reflect<Animation::MeshSkinAssetType>(
			Animation::MeshSkinAssetType::AssetFormat.assetTypeGuid,
			MAKE_UNICODE_LITERAL("Mesh Skin"),
			Reflection::TypeFlags{},
			Reflection::Tags{Asset::Tags::AssetType}
		);
	};
}
