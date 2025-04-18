#include "PhysicsCore/Components/Vehicles/Axle.h"
#include <PhysicsCore/Components/Vehicles/Vehicle.h>
#include <PhysicsCore/Components/Vehicles/Wheel.h>

#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/HierarchyComponent.inl>
#include <Common/Serialization/Reader.h>
#include <Common/Reflection/Registry.inl>

#include <3rdparty/jolt/Jolt.h>
#include <3rdparty/jolt/Physics/PhysicsSystem.h>

namespace ngine::Physics
{
	Axle::Axle(const Axle& templateComponent, const Cloner& cloner)
		: BaseType(templateComponent, cloner)
		, m_enginePowerRatio(templateComponent.m_enginePowerRatio)
		, m_leftRightPowerRatio(templateComponent.m_leftRightPowerRatio)
		, m_differentialRatio(templateComponent.m_differentialRatio)
		, m_antiRollBarStiffness(templateComponent.m_antiRollBarStiffness)
	{
	}

	Axle::Axle(const Deserializer& deserializer)
		: Axle(deserializer, deserializer.m_reader.FindSerializer(Reflection::GetTypeGuid<Axle>().ToString().GetView()))
	{
	}

	Axle::Axle(const Deserializer& deserializer, const Optional<Serialization::Reader> typeSerializer)
		: BaseType(deserializer)
		, m_enginePowerRatio(
				typeSerializer.IsValid() ? typeSerializer->ReadWithDefaultValue<Math::Ratiof>("powerRatio", 30_percent) : Math::Ratiof(30_percent)
			)
		, m_leftRightPowerRatio(
				typeSerializer.IsValid() ? typeSerializer->ReadWithDefaultValue<Math::Ratiof>("powerSplit", 50_percent) : Math::Ratiof(50_percent)
			)
		, m_differentialRatio(
				typeSerializer.IsValid() ? typeSerializer->ReadWithDefaultValue<Math::Ratiof>("differentialRatio", 342_percent)
																 : Math::Ratiof(342_percent)
			)
		, m_antiRollBarStiffness(
				typeSerializer.IsValid() ? typeSerializer->ReadWithDefaultValue<Math::Torquef>("antiRollBarStiffness", 500_newtonmeters)
																 : Math::Torquef{500_newtonmeters}
			)
	{
	}

	Axle::Axle(Initializer&& initializer)
		: BaseType(Forward<Initializer>(initializer))
	{
	}

	void Axle::OnCreated()
	{
		if (const Optional<Vehicle*> pVehicle = FindFirstParentOfType<Vehicle>())
		{
			pVehicle->AddAxle(*this);
		}
	}

	void Axle::OnDestroying()
	{
		if (const Optional<Vehicle*> pVehicle = FindFirstParentOfType<Vehicle>())
		{
			pVehicle->RemoveAxle(m_vehicleAxleIndex);
			m_vehicleAxleIndex = Math::NumericLimits<uint16>::Max;
		}
	}

	void Axle::AddWheel(Wheel& wheel)
	{
		if (const Optional<Vehicle*> pVehicle = FindFirstParentOfType<Vehicle>())
		{
			pVehicle->AddWheel(wheel, *this);
		}

		if (m_leftWheel.IsInvalid())
		{
			m_leftWheel = wheel;
			return;
		}

		const Math::Vector3f leftWheelToNewWheelDirection = wheel.GetRelativeLocation() - m_leftWheel->GetRelativeLocation();
		const Math::Vector3f rightDirection = GetRelativeRightDirection();
		const Math::Vector3f leftToNewDirectionNormalized = leftWheelToNewWheelDirection.GetNormalizedSafe(rightDirection);

		if (leftToNewDirectionNormalized.Dot(rightDirection) > 0.f)
		{
			m_rightWheel = wheel;
		}
		else
		{
			m_rightWheel = m_leftWheel;
			m_leftWheel = wheel;
		}
	}

	void Axle::RemoveWheel(Wheel& wheel)
	{
		if (const Optional<Vehicle*> pVehicle = FindFirstParentOfType<Vehicle>())
		{
			pVehicle->RemoveWheel(wheel.m_wheelIndex, m_vehicleAxleIndex);
			wheel.m_wheelIndex = Math::NumericLimits<uint16>::Max;
		}

		if (m_leftWheel == &wheel)
		{
			m_leftWheel = Invalid;
		}
		else if (m_rightWheel == &wheel)
		{
			m_rightWheel = Invalid;
		}
	}

	[[maybe_unused]] const bool wasAxleRegistered = Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<Axle>>::Make());
	[[maybe_unused]] const bool wasAxleTypeRegistered = Reflection::Registry::RegisterType<Axle>();
}
