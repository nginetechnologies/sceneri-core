#include "Components/Boid.h"

#include <Common/Reflection/Registry.inl>
#include <Engine/Entity/ComponentType.h>

namespace ngine::GameFramework
{
	Boid::Boid(Initializer&&)
	{
	}

	Boid::Boid(const Deserializer&)
	{
	}

	Boid::Boid(const Boid& templateComponent, const Cloner&)
		: m_state(templateComponent.m_state)
	{
	}

	[[maybe_unused]] const bool wasBoidRegistered = Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<Boid>>::Make());
	[[maybe_unused]] const bool wasBoidTypeRegistered = Reflection::Registry::RegisterType<Boid>();
	[[maybe_unused]] const bool wasBoidStateTypeRegistered = Reflection::Registry::RegisterType<Boid::StartState>();
}
