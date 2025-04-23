#include "TempVisualScripting/Receivers/SimulationReceiver.h"
#include "Reset/SimulationComponent.h"

#include <PhysicsCore/Components/Data/SceneComponent.h>
#include <PhysicsCore/Components/Data/BodyComponent.h>

#include <Engine/Entity/Component3D.h>
#include <Engine/Entity/HierarchyComponent.inl>
#include <Engine/Entity/Data/Component.inl>
#include <Engine/Entity/Component3D.inl>
#include <Engine/Entity/RootSceneComponent.h>
#include <Common/Reflection/Registry.inl>

namespace ngine::GameFramework::Signal::Receivers
{
	Simulation::Simulation(const Simulation& templateComponent, const Cloner& cloner)
		: Receiver(templateComponent, cloner)
	{
	}
	Simulation::Simulation(const Deserializer& deserializer)
		: Receiver(deserializer)
	{
	}
	Simulation::Simulation(Initializer&& initializer)
		: Receiver(Forward<Initializer>(initializer))
	{
	}

	void Simulation::OnCreated(Entity::Component3D& owner)
	{
		Optional<GameFramework::Data::Simulation*> pSimulation = owner.FindDataComponentOfType<GameFramework::Data::Simulation>();
		if (pSimulation.IsInvalid())
		{
			pSimulation = owner.CreateDataComponent<GameFramework::Data::Simulation>();
		}

		Assert(pSimulation.IsValid());
		if (LIKELY(pSimulation.IsValid()))
		{
			switch (m_mode)
			{
				case Mode::Latch:
				case Mode::Relay:
					pSimulation->SetAutostart(false);
					break;
				case Mode::InverseLatch:
				case Mode::InverseRelay:
					pSimulation->SetAutostart(true);
					break;
			}
		}
	}

	void Simulation::SetMode(Entity::Component3D& owner, Mode mode)
	{
		Receiver::SetMode(owner, mode);
		const Optional<GameFramework::Data::Simulation*> pSimulation = owner.FindDataComponentOfType<GameFramework::Data::Simulation>();
		Assert(pSimulation.IsValid());
		if (LIKELY(pSimulation.IsValid()))
		{
			switch (m_mode)
			{
				case Mode::Latch:
				case Mode::Relay:
					pSimulation->SetAutostart(false);
					break;
				case Mode::InverseLatch:
				case Mode::InverseRelay:
					pSimulation->SetAutostart(true);
					break;
			}
		}
	}

	void Simulation::Activate(Entity::Component3D& owner)
	{
		owner.ResumeSimulation(owner.GetSceneRegistry());
	}

	void Simulation::Deactivate(Entity::Component3D& owner)
	{
		owner.PauseSimulation(owner.GetSceneRegistry());
	}

	[[maybe_unused]] const bool wasSimulationReceiverRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<Simulation>>::Make());
	[[maybe_unused]] const bool wasSimulationReceiverTypeRegistered = Reflection::Registry::RegisterType<Simulation>();
}
