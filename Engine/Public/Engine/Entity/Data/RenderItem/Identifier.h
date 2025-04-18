#pragma once

#include <Engine/Entity/RenderItemIdentifier.h>
#include <Engine/Entity/Data/HierarchyComponent.h>

namespace ngine::Entity::Data::RenderItem
{
	struct Identifier final : public HierarchyComponent
	{
		using BaseType = HierarchyComponent;
		using InstanceIdentifier = Entity::RenderItemIdentifier;

		Identifier(const Entity::RenderItemIdentifier identifier)
			: m_identifier(identifier)
		{
		}

		[[nodiscard]] operator Entity::RenderItemIdentifier() const
		{
			return m_identifier;
		}

		void OnDestroying(Entity::HierarchyComponentBase&);
	protected:
		friend struct Reflection::ReflectedType<Entity::Data::RenderItem::Identifier>;

		const Entity::RenderItemIdentifier m_identifier;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::Data::RenderItem::Identifier>
	{
		inline static constexpr auto Type = Reflection::Reflect<Entity::Data::RenderItem::Identifier>(
			"7458d552-8309-4c25-9a34-d8143d9cb7e5"_guid,
			MAKE_UNICODE_LITERAL("Render Item Identifier"),
			TypeFlags::DisableDynamicInstantiation | TypeFlags::DisableDynamicCloning | TypeFlags::DisableDynamicDeserialization |
				TypeFlags::DisableWriteToDisk | TypeFlags::DisableUserInterfaceInstantiation | TypeFlags::DisableDeletionFromUserInterface
		);
	};
}
