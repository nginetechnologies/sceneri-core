#include "Components/Player/Health.h"

#include <Common/Reflection/Registry.inl>
#include <Engine/Entity/ComponentType.h>

namespace ngine::GameFramework
{
	Health::Health(const Deserializer&)
	{
	}

	Health::Health(const Health& templateComponent, const Cloner&)
		: m_health(templateComponent.m_health)
		, m_maximum(templateComponent.m_maximum)
	{
	}

	Health::Health(Initializer&&)
	{
	}

	void Health::OnCreated()
	{
		m_health = m_maximum;
	}

	[[maybe_unused]] const bool wasHealthRegistered = Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<Health>>::Make());
	[[maybe_unused]] const bool wasHealthTypeRegistered = Reflection::Registry::RegisterType<Health>();
}
