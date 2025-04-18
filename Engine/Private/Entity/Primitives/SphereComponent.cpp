#include "Entity/Primitives/SphereComponent.h"

#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/ComponentRegistry.h>
#include <Engine/Entity/ComponentTypeSceneData.h>

#include <Renderer/Renderer.h>
#include <Renderer/Assets/StaticMesh/MeshCache.h>
#include <Renderer/Assets/StaticMesh/Primitives/Primitives.h>

#include <Common/Reflection/Registry.inl>

namespace ngine::Entity::Primitives
{
	SphereComponent::SphereComponent(const SphereComponent& templateComponent, const Cloner& cloner)
		: StaticMeshComponent(templateComponent, cloner)
	{
	}

	SphereComponent::SphereComponent(const Deserializer& deserializer)
		: StaticMeshComponent(
				deserializer,
				System::Get<Rendering::Renderer>().GetMeshCache().FindOrRegisterAsset(ngine::Primitives::SphereMeshAssetGuid),
				deserializer.m_reader.FindSerializer(Reflection::GetTypeGuid<StaticMeshComponent>().ToString().GetView())
			)
	{
	}

	SphereComponent::SphereComponent(Initializer&& initializer)
		: StaticMeshComponent(StaticMeshComponent::Initializer{
				RenderItemComponent::Initializer{initializer},
				System::Get<Rendering::Renderer>().GetMeshCache().FindOrRegisterAsset(ngine::Primitives::SphereMeshAssetGuid),
				initializer.m_materialInstanceIdentifier
			})
	{
	}

	[[maybe_unused]] const bool wasSphereRegistered = Entity::ComponentRegistry::Register(UniquePtr<ComponentType<SphereComponent>>::Make());
	[[maybe_unused]] const bool wasSphereTypeRegistered = Reflection::Registry::RegisterType<SphereComponent>();
}
