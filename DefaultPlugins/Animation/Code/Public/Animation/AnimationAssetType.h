#pragma once

#include <Common/Asset/AssetFormat.h>
#include <Common/Asset/AssetTypeTag.h>
#include <Common/Reflection/Type.h>

namespace ngine::Animation
{
	struct AnimationAssetType
	{
		inline static constexpr Asset::Format AssetFormat = {
			"{2D002E95-6D4F-48BE-9C62-8CEA8F1E7CA9}"_guid, MAKE_PATH(".anim.nasset"), MAKE_PATH(".anim")
		};
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Animation::AnimationAssetType>
	{
		inline static constexpr auto Type = Reflection::Reflect<Animation::AnimationAssetType>(
			Animation::AnimationAssetType::AssetFormat.assetTypeGuid,
			MAKE_UNICODE_LITERAL("Animation"),
			Reflection::TypeFlags{},
			Reflection::Tags{Asset::Tags::AssetType}
		);
	};
}
