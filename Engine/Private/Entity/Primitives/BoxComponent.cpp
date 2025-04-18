#include "Entity/Primitives/BoxComponent.h"

#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/ComponentRegistry.h>
#include <Engine/Entity/ComponentTypeSceneData.h>

#include <Renderer/Renderer.h>
#include <Renderer/Assets/StaticMesh/MeshCache.h>
#include <Renderer/Assets/StaticMesh/Primitives/Primitives.h>

#include <Common/Reflection/Registry.inl>

namespace ngine::Entity::Primitives
{
	BoxComponent::BoxComponent(const BoxComponent& templateComponent, const Cloner& cloner)
		: StaticMeshComponent(templateComponent, cloner)
	{
	}

	BoxComponent::BoxComponent(const Deserializer& deserializer)
		: StaticMeshComponent(
				deserializer,
				System::Get<Rendering::Renderer>().GetMeshCache().FindOrRegisterAsset(ngine::Primitives::BoxMeshAssetGuid),
				deserializer.m_reader.FindSerializer(Reflection::GetTypeGuid<StaticMeshComponent>().ToString().GetView())
			)
	{
	}

	BoxComponent::BoxComponent(Initializer&& initializer)
		: StaticMeshComponent(StaticMeshComponent::Initializer{
				RenderItemComponent::Initializer{initializer},
				System::Get<Rendering::Renderer>().GetMeshCache().FindOrRegisterAsset(ngine::Primitives::BoxMeshAssetGuid),
				initializer.m_materialInstanceIdentifier
			})
	{
	}

	[[maybe_unused]] const bool wasBoxRegistered = Entity::ComponentRegistry::Register(UniquePtr<ComponentType<BoxComponent>>::Make());
	[[maybe_unused]] const bool wasBoxTypeRegistered = Reflection::Registry::RegisterType<BoxComponent>();
}
