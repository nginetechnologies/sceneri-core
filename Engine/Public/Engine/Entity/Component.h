#pragma once

#include <Common/Guid.h>
#include <Common/Reflection/PropertyOwner.h>

#include <Common/Reflection/Type.h>
#include <Engine/Entity/ComponentTypeExtension.h>

namespace ngine::Entity
{
	struct Component : public Reflection::PropertyOwner
	{
		Component() = default;
		Component(const Component&) = delete;
		Component& operator=(const Component&) = delete;
		Component(Component&&) = delete;
		Component& operator=(Component&&) = delete;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::Component>
	{
		inline static constexpr auto Type = Reflection::Reflect<Entity::Component>(
			"3F7B61BF-EF50-45EC-83E1-AD0AC14BA4C4"_guid, MAKE_UNICODE_LITERAL("Component"), TypeFlags::IsAbstract
		);
	};
}
