#pragma once

#include <Common/Asset/AssetFormat.h>
#include <Common/Asset/AssetTypeTag.h>
#include <Common/Reflection/Type.h>

namespace ngine::Animation
{
	struct SkeletonAssetType
	{
		inline static constexpr Asset::Format AssetFormat = {
			"{D36708F1-71F9-4334-8146-62C743C39970}"_guid, MAKE_PATH(".skel.nasset"), MAKE_PATH(".skel")
		};
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Animation::SkeletonAssetType>
	{
		inline static constexpr auto Type = Reflection::Reflect<Animation::SkeletonAssetType>(
			Animation::SkeletonAssetType::AssetFormat.assetTypeGuid,
			MAKE_UNICODE_LITERAL("Skeleton"),
			Reflection::TypeFlags{},
			Reflection::Tags{Asset::Tags::AssetType}
		);
	};
}
