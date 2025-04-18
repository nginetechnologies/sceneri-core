#pragma once

#include <Common/Asset/AssetFormat.h>
#include <Common/Asset/AssetTypeTag.h>
#include <Common/Reflection/Type.h>

namespace ngine
{
	struct Scene3DAssetType
	{
		inline static constexpr ngine::Asset::Format AssetFormat = {
			"807D69A9-56EC-4032-BB92-D081DF336672"_guid, MAKE_PATH(".scene.nasset"), {}, ngine::Asset::Format::Flags::IsCollection
		};
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Scene3DAssetType>
	{
		inline static constexpr auto Type = Reflection::Reflect<Scene3DAssetType>(
			Scene3DAssetType::AssetFormat.assetTypeGuid,
			MAKE_UNICODE_LITERAL("3D Scene"),
			Reflection::TypeFlags{},
			Reflection::Tags{Asset::Tags::AssetType}
		);
	};
}
