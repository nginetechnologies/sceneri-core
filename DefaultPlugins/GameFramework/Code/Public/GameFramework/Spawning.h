#pragma once

#include <Common/Math/WorldCoordinate.h>
#include <Common/Math/Transform.h>
#include <Common/Function/Function.h>
#include <Common/Asset/Guid.h>

namespace ngine
{
	struct Scene3D;
}

namespace ngine::Entity
{
	struct Component3D;
	struct SceneComponent;
}

namespace ngine::GameFramework
{
	struct SpawnInitializer
	{
		Scene3D& m_rootScene;
		Optional<Entity::SceneComponent*> m_pCreator;

		Math::WorldTransform m_worldTransform{Math::Identity};
	};

	using SpawnAssetCallback = Function<void(Optional<Entity::Component3D*>), 24>;

	bool SpawnAsset(
		const Asset::Guid assetGuid,
		SpawnInitializer&& initializer,
		SpawnAssetCallback&& callback =
			[](Optional<Entity::Component3D*>)
		{
		}
	);
}
