#pragma once

#include <Components/Signals/Receiver.h>

#include <Engine/Tag/TagMask.h>

namespace ngine::Entity
{
	struct Component3D;
}

namespace ngine::GameFramework::Signal::Receivers
{
	//! Receiver that starts / stop simulation on signal
	struct Simulation final : public Receiver
	{
		static constexpr Guid TypeGuid = "c489ab90-32b2-4d08-a12b-0b515249ee80"_guid;

		using BaseType = Receiver;

		Simulation(const Simulation& templateComponent, const Cloner& cloner);
		Simulation(const Deserializer& deserializer);
		Simulation(Initializer&& initializer);

		void OnCreated(Entity::Component3D& owner);
	protected:
		friend struct Reflection::ReflectedType<Simulation>;

		virtual void SetMode(Entity::Component3D& owner, Mode mode) override final;

		virtual void Activate(Entity::Component3D& owner) override final;
		virtual void Deactivate(Entity::Component3D& owner) override final;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<GameFramework::Signal::Receivers::Simulation>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::Signal::Receivers::Simulation>(
			GameFramework::Signal::Receivers::Simulation::TypeGuid,
			MAKE_UNICODE_LITERAL("Simulation Receiver"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{
				Entity::ComponentTypeExtension{Entity::ComponentTypeFlags(), {}, "5bfbc860-9009-471e-8cd5-2c7a6815a5bf"_guid},
			}
		);
	};
}
