#pragma once

#include <Engine/Entity/Data/Component.h>
#include <Engine/Entity/ComponentTypeIdentifier.h>

#include <Common/Storage/Identifier.h>

namespace ngine::Entity::Data
{
	struct TypeIndex final : public Component
	{
		TypeIndex(const Deserializer& deserializer) = delete;
		TypeIndex(const TypeIndex&, const Cloner&) = delete;

		TypeIndex(ComponentTypeIdentifier identifier)
			: m_index(identifier.GetFirstValidIndex())
		{
		}

		[[nodiscard]] ComponentTypeIdentifier GetIdentifier() const
		{
			return ComponentTypeIdentifier::MakeFromValidIndex(m_index);
		}

		[[nodiscard]] operator ComponentTypeIdentifier() const
		{
			return ComponentTypeIdentifier::MakeFromValidIndex(m_index);
		}

		TypeIndex& operator=(ComponentTypeIdentifier identifier)
		{
			m_index = identifier.GetIndex();
			return *this;
		}
	protected:
		friend struct Reflection::ReflectedType<Entity::Data::TypeIndex>;
	private:
		ComponentTypeIdentifier::IndexType m_index;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::Data::TypeIndex>
	{
		inline static constexpr auto Type = Reflection::Reflect<Entity::Data::TypeIndex>(
			"bbe6ede7-df08-43de-993c-b5b99596e1c7"_guid,
			MAKE_UNICODE_LITERAL("Type Index"),
			TypeFlags::DisableDynamicInstantiation | TypeFlags::DisableDynamicCloning | TypeFlags::DisableDynamicDeserialization |
				TypeFlags::DisableWriteToDisk | TypeFlags::DisableUserInterfaceInstantiation | TypeFlags::DisableDeletionFromUserInterface
		);
	};
}
