#pragma once

#include <Common/Asset/AssetFormat.h>
#include <Common/Asset/AssetTypeTag.h>
#include <Common/Reflection/Type.h>

namespace ngine::Physics
{
	struct MeshAssetType
	{
		inline static constexpr ngine::Asset::Format AssetFormat = {
			"89D6E4D9-CDFA-4A7E-B0E8-A9A835F65CBF"_guid, MAKE_PATH(".physmesh.nasset"), MAKE_PATH(".physmesh")
		};
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Physics::MeshAssetType>
	{
		inline static constexpr auto Type = Reflection::Reflect<Physics::MeshAssetType>(
			Physics::MeshAssetType::AssetFormat.assetTypeGuid,
			MAKE_UNICODE_LITERAL("Physical Mesh"),
			Reflection::TypeFlags{},
			Reflection::Tags{Asset::Tags::AssetType}
		);
	};
}
