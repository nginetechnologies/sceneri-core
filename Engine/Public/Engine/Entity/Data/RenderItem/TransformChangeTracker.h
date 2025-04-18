#pragma once

#include <Engine/Entity/Data/Component.h>
#include <Engine/Entity/RenderItemIdentifier.h>

namespace ngine::Entity::Data::RenderItem
{
	struct TransformChangeTracker final : public Component
	{
		using InstanceIdentifier = Entity::RenderItemIdentifier;

		TransformChangeTracker() = default;

		void OnTransformChanged()
		{
			m_changeTracker++;
		}

		using ValueType = uint16;

		[[nodiscard]] bool operator!=(const ValueType value) const
		{
			return m_changeTracker != value;
		}
		[[nodiscard]] ValueType GetValue() const
		{
			return m_changeTracker;
		}
	protected:
		friend struct Reflection::ReflectedType<Entity::Data::RenderItem::TransformChangeTracker>;

		uint16 m_changeTracker = 0u;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::Data::RenderItem::TransformChangeTracker>
	{
		inline static constexpr auto Type = Reflection::Reflect<Entity::Data::RenderItem::TransformChangeTracker>(
			"321fa098-b2d7-4e7a-876f-33bdee14ad17"_guid,
			MAKE_UNICODE_LITERAL("Render Item Transform Change Tracker"),
			TypeFlags::DisableDynamicInstantiation | TypeFlags::DisableDynamicCloning | TypeFlags::DisableDynamicDeserialization |
				TypeFlags::DisableWriteToDisk | TypeFlags::DisableUserInterfaceInstantiation | TypeFlags::DisableDeletionFromUserInterface
		);
	};
}
