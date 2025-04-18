#include "Entity/RootComponent.h"

#include "Engine/Entity/ComponentType.h"
#include "Engine/Entity/HierarchyComponent.inl"

#include <Common/Reflection/Registry.inl>

namespace ngine::Entity
{
	RootComponent::RootComponent(SceneRegistry& sceneRegistry)
		: HierarchyComponent(sceneRegistry.AcquireNewComponentIdentifier(), nullptr, sceneRegistry)
		, m_sceneRegistry(sceneRegistry)
	{
	}

	template struct HierarchyComponent<HierarchyComponentBase>;

	[[maybe_unused]] const bool wasRootComponentTypeRegistered = Reflection::Registry::RegisterType<RootComponent>();
	[[maybe_unused]] const bool wasRootComponentRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<ComponentType<RootComponent>>::Make());
}
