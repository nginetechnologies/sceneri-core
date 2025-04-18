#pragma once

#include <Engine/Entity/Data/Component.h>
#include <Engine/Entity/ComponentIdentifier.h>
#include <Engine/Entity/HierarchyComponentBase.h>

namespace ngine::Context::Data
{
	struct Component;

	struct Reference : public Entity::Data::Component
	{
		using InstanceIdentifier = TIdentifier<uint32, 8>;
		using BaseType = Entity::Data::Component;

		struct Initializer : public Entity::Data::Component::Initializer
		{
			using BaseType = Entity::Data::Component::Initializer;

			Initializer(BaseType&& initializer, const Entity::ComponentIdentifier componentIdentifier)
				: BaseType(Forward<BaseType>(initializer))
				, m_componentIdentifier(componentIdentifier)
			{
			}

			Entity::ComponentIdentifier m_componentIdentifier;
		};

		Reference(Initializer&& initializer);
		virtual ~Reference();

		[[nodiscard]] Optional<Context::Data::Component*> GetComponent(Entity::SceneRegistry& registry) const;
		[[nodiscard]] Entity::ComponentIdentifier GetReferencedComponentIdentifier() const
		{
			return m_componentIdentifier;
		}
	protected:
		friend struct Reflection::ReflectedType<Reference>;

		Entity::ComponentIdentifier m_componentIdentifier;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Context::Data::Reference>
	{
		inline static constexpr auto Type = Reflection::Reflect<Context::Data::Reference>(
			"E2F73EE4-BF07-407C-8C1C-39A482076B49"_guid,
			MAKE_UNICODE_LITERAL("Context Reference"),
			TypeFlags::DisableDynamicInstantiation | TypeFlags::DisableDynamicCloning | TypeFlags::DisableDynamicDeserialization |
				TypeFlags::DisableWriteToDisk
		);
	};
}
