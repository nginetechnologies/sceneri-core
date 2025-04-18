#pragma once

#include <Engine/Entity/Data/Component.h>

namespace ngine::Entity::Data
{
	//! Indicates a components preferred location in its parent's child container
	struct PreferredIndex final : public Component
	{
		using ChildIndex = uint16;

		PreferredIndex(const Deserializer& deserializer) = delete;
		PreferredIndex(const PreferredIndex&, const Cloner&) = delete;
		PreferredIndex(const ChildIndex parentIndex)
			: m_parentIndex(parentIndex)
		{
		}
		PreferredIndex& operator=(const ChildIndex parentIndex)
		{
			m_parentIndex = parentIndex;
			return *this;
		}

		[[nodiscard]] ChildIndex Get() const
		{
			return m_parentIndex;
		}
	protected:
		friend struct Reflection::ReflectedType<Entity::Data::PreferredIndex>;
		ChildIndex m_parentIndex;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::Data::PreferredIndex>
	{
		inline static constexpr auto Type = Reflection::Reflect<Entity::Data::PreferredIndex>(
			"{58552A1C-4F65-48C1-96B7-25BE6980A0FD}"_guid,
			MAKE_UNICODE_LITERAL("Component Preferred Index"),
			TypeFlags::DisableDynamicInstantiation | TypeFlags::DisableDynamicCloning | TypeFlags::DisableDynamicDeserialization |
				TypeFlags::DisableWriteToDisk | TypeFlags::DisableUserInterfaceInstantiation | TypeFlags::DisableDeletionFromUserInterface
		);
	};
}
