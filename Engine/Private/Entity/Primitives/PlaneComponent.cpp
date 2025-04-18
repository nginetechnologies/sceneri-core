#include "Entity/Primitives/PlaneComponent.h"

#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/ComponentRegistry.h>
#include <Engine/Entity/ComponentTypeSceneData.h>

#include <Renderer/Renderer.h>
#include <Renderer/Assets/StaticMesh/MeshCache.h>
#include <Renderer/Assets/StaticMesh/Primitives/Primitives.h>

#include <Common/Reflection/Registry.inl>

namespace ngine::Entity::Primitives
{
	PlaneComponent::PlaneComponent(const PlaneComponent& templateComponent, const Cloner& cloner)
		: StaticMeshComponent(templateComponent, cloner)
	{
	}

	PlaneComponent::PlaneComponent(const Deserializer& deserializer)
		: StaticMeshComponent(
				deserializer,
				System::Get<Rendering::Renderer>().GetMeshCache().FindOrRegisterAsset(ngine::Primitives::PlaneMeshAssetGuid),
				deserializer.m_reader.FindSerializer(Reflection::GetTypeGuid<StaticMeshComponent>().ToString().GetView())
			)
	{
	}

	PlaneComponent::PlaneComponent(Initializer&& initializer)
		: StaticMeshComponent(StaticMeshComponent::Initializer{
				RenderItemComponent::Initializer{initializer},
				System::Get<Rendering::Renderer>().GetMeshCache().FindOrRegisterAsset(ngine::Primitives::PlaneMeshAssetGuid),
				initializer.m_materialInstanceIdentifier
			})
	{
	}

	[[maybe_unused]] const bool wasPlaneRegistered = Entity::ComponentRegistry::Register(UniquePtr<ComponentType<PlaneComponent>>::Make());
	[[maybe_unused]] const bool wasPlaneTypeRegistered = Reflection::Registry::RegisterType<PlaneComponent>();
}
