#include "Components/Creator.h"

#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/Scene/SceneComponent.h>
#include <Engine/Entity/ComponentSoftReference.inl>
#include <Engine/Entity/Component3D.inl>

#include <Common/Reflection/Registry.inl>

namespace ngine::GameFramework
{
	Creator::Creator(Initializer&& initializer)
		: m_creator(initializer.m_creator, initializer.m_creator.GetSceneRegistry())
	{
	}

	Optional<Entity::SceneComponent*> Creator::Find(const Entity::SceneRegistry& sceneRegistry) const
	{
		return m_creator.Find<Entity::SceneComponent>(sceneRegistry);
	}

	Optional<Entity::SceneComponent*> FindCreator(const Entity::Component3D& component)
	{
		if (Optional<Creator*> pCreator = component.FindFirstDataComponentImplementingType<Creator>())
		{
			return pCreator->Find(component.GetSceneRegistry());
		}

		return Invalid;
	}

	[[maybe_unused]] const bool wasCreatorRegistered = Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<Creator>>::Make());
	[[maybe_unused]] const bool wasCreatorTypeRegistered = Reflection::Registry::RegisterType<Creator>();
}
