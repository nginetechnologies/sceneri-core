#pragma once

#include <Engine/Entity/Data/Component.h>

#include <Common/Serialization/Guid.h>

namespace ngine::GameFramework::Data
{
	struct Simulation final : public Entity::Data::Component
	{
		static constexpr Guid TypeGuid = "b84ac7bc-3b0b-44bf-ab74-31caae9d81e7"_guid;

		using BaseType = Entity::Data::Component;
		using InstanceIdentifier = TIdentifier<uint32, 9>;

		Simulation() = default;
		Simulation(const Deserializer&)
		{
		}
		Simulation(const Simulation& templateComponent, const Cloner&)
			: m_flags(templateComponent.m_flags)
		{
		}

		void SetAutostart(bool isAutostart)
		{
			m_flags.Set(Flags::Autostart, isAutostart);
		}
		[[nodiscard]] bool IsAutostart() const
		{
			return m_flags.IsSet(Flags::Autostart);
		}

		enum class Flags : uint8
		{
			Autostart = 1 << 0
		};
	protected:
		EnumFlags<Flags> m_flags;
	};

	ENUM_FLAG_OPERATORS(Simulation::Flags);
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<ngine::GameFramework::Data::Simulation>
	{
		static constexpr auto Type = Reflection::Reflect<ngine::GameFramework::Data::Simulation>(
			ngine::GameFramework::Data::Simulation::TypeGuid,
			MAKE_UNICODE_LITERAL("Simulation"),
			TypeFlags::DisableUserInterfaceInstantiation | TypeFlags::DisableDynamicInstantiation | TypeFlags::DisableWriteToDisk |
				TypeFlags::DisableDeletionFromUserInterface,
			Reflection::Tags{},
			Reflection::Properties{}
		);
	};
}
