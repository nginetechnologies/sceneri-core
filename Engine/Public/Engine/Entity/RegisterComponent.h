#pragma once

#include <Engine/Entity/ComponentType.h>

#include <Common/Reflection/Registry.inl>

namespace ngine::Entity
{
	template<typename Type>
	bool RegisterComponent()
	{
		return Reflection::Registry::RegisterType<Type>() &&
		       Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<Type>>::Make());
	}
}
