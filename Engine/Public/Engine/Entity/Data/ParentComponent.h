#pragma once

#include <Engine/Entity/Data/Component.h>

namespace ngine::Entity::Data
{
	struct Parent final : public Component
	{
		Parent(const Deserializer& deserializer) = delete;
		Parent(const Parent&, const Cloner&) = delete;
		Parent(const Entity::ComponentIdentifier::IndexType parentIndex)
			: m_parentIndex(parentIndex)
		{
		}
		Parent& operator=(const Entity::ComponentIdentifier::IndexType parentIndex)
		{
			m_parentIndex = parentIndex;
			return *this;
		}

		[[nodiscard]] Entity::ComponentIdentifier::IndexType Get() const
		{
			return m_parentIndex;
		}
		[[nodiscard]] operator Entity::ComponentIdentifier() const
		{
			return Entity::ComponentIdentifier::MakeFromValidIndex(m_parentIndex);
		}
	protected:
		friend struct Reflection::ReflectedType<Entity::Data::Parent>;
		Entity::ComponentIdentifier::IndexType m_parentIndex;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::Data::Parent>
	{
		inline static constexpr auto Type = Reflection::Reflect<Entity::Data::Parent>(
			"7416098F-7D7B-4D26-A7C3-10ACF4A52E62"_guid,
			MAKE_UNICODE_LITERAL("Component Parent"),
			TypeFlags::DisableDynamicInstantiation | TypeFlags::DisableDynamicCloning | TypeFlags::DisableDynamicDeserialization |
				TypeFlags::DisableWriteToDisk | TypeFlags::DisableUserInterfaceInstantiation | TypeFlags::DisableDeletionFromUserInterface
		);
	};
}
