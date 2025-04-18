#pragma once

#include <Engine/Entity/Data/Component.h>
#include <Engine/Entity/ComponentFlags.h>

#include <Common/AtomicEnumFlags.h>

namespace ngine::Entity::Data
{
	struct Flags final : public Component
	{
		Flags() = default;
		Flags(const EnumFlags<ComponentFlags> flags)
			: m_flags(flags)
		{
		}

		[[nodiscard]] operator EnumFlags<ComponentFlags>() const
		{
			return m_flags.GetFlags();
		}
		[[nodiscard]] operator AtomicEnumFlags<ComponentFlags>&()
		{
			return m_flags;
		}
	protected:
		friend struct Reflection::ReflectedType<Entity::Data::Flags>;
		AtomicEnumFlags<ComponentFlags> m_flags;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::Data::Flags>
	{
		inline static constexpr auto Type = Reflection::Reflect<Entity::Data::Flags>(
			"550515f2-d9d8-4802-83d8-9126335ef154"_guid,
			MAKE_UNICODE_LITERAL("Component Flags"),
			TypeFlags::DisableDynamicInstantiation | TypeFlags::DisableDynamicCloning | TypeFlags::DisableDynamicDeserialization |
				TypeFlags::DisableWriteToDisk | TypeFlags::DisableUserInterfaceInstantiation | TypeFlags::DisableDeletionFromUserInterface
		);
	};
}
