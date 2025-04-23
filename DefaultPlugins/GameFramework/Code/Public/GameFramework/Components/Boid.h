#pragma once

#include <Engine/Entity/Data/Component.h>

#include <Common/Reflection/EnumTypeExtension.h>

namespace ngine::GameFramework
{
	struct Boid final : public Entity::Data::Component
	{
		enum class StartState : uint8
		{
			Idle,
			Moving,
		};

		using BaseType = Entity::Data::Component;
		using Initializer = BaseType::DynamicInitializer;

		Boid(const Deserializer& deserializer);
		Boid(const Boid& templateComponent, const Cloner& cloner);
		Boid(Initializer&& initializer);

		[[nodiscard]] StartState GetState() const
		{
			return m_state;
		}
	private:
		friend struct Reflection::ReflectedType<GameFramework::Boid>;
		StartState m_state;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<GameFramework::Boid::StartState>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::Boid::StartState>(
			"81b08d8f-64cf-4222-a12c-3613e05c5352"_guid,
			MAKE_UNICODE_LITERAL("Boid State"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{Reflection::EnumTypeExtension{
				Reflection::EnumTypeEntry{GameFramework::Boid::StartState::Idle, MAKE_UNICODE_LITERAL("Idle")},
				Reflection::EnumTypeEntry{GameFramework::Boid::StartState::Moving, MAKE_UNICODE_LITERAL("Moving")}
			}}
		);
	};

	template<>
	struct ReflectedType<GameFramework::Boid>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::Boid>(
			"bbc174d1-f1a7-4417-b2df-cc2ea6675d0f"_guid,
			MAKE_UNICODE_LITERAL("Boid Component"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{Reflection::MakeProperty(
				MAKE_UNICODE_LITERAL("Start State"),
				"state",
				"{98807433-3098-4E58-99B1-A2E42D0844A4}"_guid,
				MAKE_UNICODE_LITERAL("Boid"),
				&GameFramework::Boid::m_state
			)}
		);
	};
}
