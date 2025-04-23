#include "Tags.h"
#include "Components/SpawnPoint.h"

#include "Engine/Entity/ComponentType.h"
#include <Engine/Entity/RootSceneComponent.h>
#include <Engine/Entity/Component3D.h>
#include <Engine/Entity/Scene/SceneComponent.h>
#include <Engine/Entity/Component3D.inl>
#include <Engine/Entity/ComponentSoftReference.inl>
#include <Engine/Entity/ComponentTypeSceneData.h>
#include <Engine/Scene/Scene3DAssetType.h>
#include <Engine/Asset/AssetManager.h>

#include <AudioCore/AudioAssetType.h>

#include <Common/Reflection/Registry.inl>

namespace ngine::GameFramework
{
	SpawnPointComponent::SpawnPointComponent(const Deserializer& deserializer)
		: Component3D(deserializer)
	{
	}

	SpawnPointComponent::SpawnPointComponent(const SpawnPointComponent& templateComponent, const Cloner& cloner)
		: Component3D(templateComponent, cloner)
		, m_assetGuid(templateComponent.m_assetGuid)
	{
	}

	SpawnPointComponent::SpawnPointComponent(Initializer&& initializer)
		: Component3D(Forward<Initializer>(initializer))
	{
	}

	void SpawnPointComponent::SetSpawnedAsset(const Asset::Picker asset)
	{
		m_assetGuid = asset.GetAssetGuid();
		OnChanged();
	}

	Asset::Picker SpawnPointComponent::GetSpawnedAsset() const
	{
		return Asset::Picker{Asset::Reference{m_assetGuid, Scene3DAssetType::AssetFormat.assetTypeGuid}, {Tags::PlayerTagGuid}};
	}

	[[maybe_unused]] const bool wasSpawnPointRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<SpawnPointComponent>>::Make());
	[[maybe_unused]] const bool wasSpawnPointTypeRegistered = Reflection::Registry::RegisterType<SpawnPointComponent>();
}
