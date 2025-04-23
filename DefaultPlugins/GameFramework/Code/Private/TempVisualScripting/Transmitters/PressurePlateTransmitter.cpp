#include "TempVisualScripting/Transmitters/PressurePlateTransmitter.h"
#include "Tags.h"

#include <Engine/Entity/Data/Tags.h>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/Component3D.inl>
#include <Engine/Entity/Component3D.h>
#include <Engine/Entity/Scene/SceneComponent.h>
#include <Engine/Entity/RootSceneComponent.h>
#include <Reset/ResetComponent.h>

#include <Engine/Entity/Serialization/ComponentReference.h>

#include <Common/Serialization/Reader.h>
#include <Common/Reflection/Registry.inl>

#include <Components/Player/Player.h>
#include <Components/Signals/Transmitter.h>

#include <Engine/Entity/HierarchyComponent.inl>

#include <PhysicsCore/Components/Data/SceneComponent.h>
#include <PhysicsCore/Components/Data/BodyComponent.h>

namespace ngine::GameFramework::Signal::Transmitters
{
	PressurePlate::PressurePlate(Initializer&& initializer)
		: SensorComponent(SensorComponent::Initializer(Forward<SensorComponent::BaseType::Initializer>(initializer), Tag::Mask()))
	{
		CreateDataComponent<Transmitter>(Transmitter::Initializer{Entity::Data::Component3D::DynamicInitializer{*this, GetSceneRegistry()}});
	}

	PressurePlate::PressurePlate(const PressurePlate& templateComponent, const Cloner& cloner)
		: SensorComponent(templateComponent, cloner)
		, m_massThreshold(templateComponent.m_massThreshold)
	{
	}

	PressurePlate::PressurePlate(const Deserializer& deserializer)
		: SensorComponent(deserializer)
	{
	}

	void PressurePlate::OnCreated()
	{
		SensorComponent::OnCreated();

		SensorComponent::OnComponentEntered.Add(*this, &PressurePlate::OnComponentDetected);
		SensorComponent::OnComponentLost.Add(*this, &PressurePlate::OnComponentLost);
	}

	// TODO: Constraint (-> trapdoor), change constraint from enable / disable -> simulation
	// - basically we want enable / disable = stuff like visibility, simulation = updates & effects
	// TODO: Door (just make this a constraint -> lock?)
	// Maybe constraint simulation pause -> lock? disable = remove constraint

	// Fix asset importing bugs / workflow after
	// Once these work, move on to MP.

	void PressurePlate::CreateReset()
	{
		if (!HasAnyDataComponentsImplementingType<Data::Reset>(GetSceneRegistry()))
		{
			CreateDataComponent<Data::Reset>(Data::Reset::Initializer{
				Entity::Data::Component3D::DynamicInitializer{*this, GetSceneRegistry()},
				[this](Entity::Component3D&)
				{
					m_currentMass = 0_kilograms;
					m_state = State::Inactive;
				},
				*this
			});
		}
	}
	void PressurePlate::OnComponentDetected(SensorComponent&, Optional<Entity::Component3D*> pComponent)
	{
		if (pComponent != nullptr)
		{
			if (Entity::Component3D::DataComponentResult<Physics::Data::Body> result = pComponent->GetParentSceneComponent()->FindFirstDataComponentOfTypeInChildrenRecursive<Physics::Data::Body>())
			{
				Optional<Physics::Data::Scene*> pPhysicsScene = pComponent->GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();

				if (Optional<Math::Massf> mass = result.m_pDataComponent->GetMass(*pPhysicsScene))
				{
					CreateReset();
					m_currentMass += *mass;
				}
			}

			if (m_currentMass > m_massThreshold && m_state == State::Inactive)
			{
				const Optional<Transmitter*> pTransmitter = FindDataComponentOfType<Transmitter>();
				Assert(pTransmitter.IsValid());
				if (LIKELY(pTransmitter.IsValid()))
				{
					pTransmitter->Start(*this);
				}

				m_state = State::Active;
			}
		}
	}

	void PressurePlate::OnComponentLost(SensorComponent&, Optional<Entity::Component3D*> pComponent)
	{
		if (pComponent != nullptr)
		{
			if (Entity::Component3D::DataComponentResult<Physics::Data::Body> result = pComponent->GetParentSceneComponent()->FindFirstDataComponentOfTypeInChildrenRecursive<Physics::Data::Body>())
			{
				Optional<Physics::Data::Scene*> pPhysicsScene = pComponent->GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();

				if (Optional<Math::Massf> mass = result.m_pDataComponent->GetMass(*pPhysicsScene))
				{
					m_currentMass -= *mass;
				}
			}

			if (m_currentMass <= m_massThreshold && m_state == State::Active)
			{
				const Optional<Transmitter*> pTransmitter = FindDataComponentOfType<Transmitter>();
				Assert(pTransmitter.IsValid());
				if (LIKELY(pTransmitter.IsValid()))
				{
					pTransmitter->Stop(*this);
				}

				m_state = State::Inactive;
			}
		}
	}

	[[maybe_unused]] const bool wasPressurePlateRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<PressurePlate>>::Make());
	[[maybe_unused]] const bool wasPressurePlateTypeRegistered = Reflection::Registry::RegisterType<PressurePlate>();
}
