#include "PhysicsCore/Components/Vehicles/Wheel.h"
#include "PhysicsCore/Components/Vehicles/Axle.h"
#include "PhysicsCore/Components/Vehicles/Vehicle.h"

#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/HierarchyComponent.inl>
#include <Engine/Entity/Data/LocalTransform3D.h>
#include <Common/Serialization/Reader.h>
#include <Common/Reflection/Registry.inl>
#include <Common/Math/IsEquivalentTo.h>

#include <3rdparty/jolt/Jolt.h>
#include <3rdparty/jolt/Physics/PhysicsSystem.h>
#include <PhysicsCore/Components/Vehicles/Vehicle.h>

namespace ngine::Physics
{
	Wheel::Wheel(const Wheel& templateComponent, const Cloner& cloner)
		: BaseType(templateComponent, cloner)
		, m_radius(templateComponent.m_radius)
		, m_width(templateComponent.m_width)
		, m_suspensionMinimumLength(templateComponent.m_suspensionMinimumLength)
		, m_suspensionMaximumLength(templateComponent.m_suspensionMaximumLength)
		, m_suspensionStiffness(templateComponent.m_suspensionStiffness)
		, m_suspensionDamping(templateComponent.m_suspensionDamping)
		, m_maximumSteeringAngle(templateComponent.m_maximumSteeringAngle)
		, m_handbrakeMaximumTorque(templateComponent.m_handbrakeMaximumTorque)
		, m_forwardGrip(templateComponent.m_forwardGrip)
		, m_sidewaysGrip(templateComponent.m_sidewaysGrip)
		, m_defaultRotation(templateComponent.m_defaultRotation)
	{
	}

	Wheel::Wheel(const Deserializer& deserializer)
		: Wheel(deserializer, deserializer.m_reader.FindSerializer(Reflection::GetTypeGuid<Wheel>().ToString().GetView()))
	{
	}

	Wheel::Wheel(const Deserializer& deserializer, const Optional<Serialization::Reader> typeSerializer)
		: BaseType(deserializer)
		, m_radius(
				typeSerializer.IsValid() ? typeSerializer->ReadWithDefaultValue<Math::Lengthf>("radius", Math::Lengthf{0.2_meters})
																 : Math::Lengthf{0.2_meters}
			)
		, m_width(
				typeSerializer.IsValid() ? typeSerializer->ReadWithDefaultValue<Math::Lengthf>("width", Math::Lengthf{0.4_meters})
																 : Math::Lengthf{0.4_meters}
			)
		, m_suspensionMinimumLength(
				typeSerializer.IsValid() ? typeSerializer->ReadWithDefaultValue<Math::Lengthf>("minSuspensionLength", Math::Lengthf{0.1_meters})
																 : Math::Lengthf{0.1_meters}
			)
		, m_suspensionMaximumLength(
				typeSerializer.IsValid() ? typeSerializer->ReadWithDefaultValue<Math::Lengthf>("maxSuspensionLength", Math::Lengthf{0.3_meters})
																 : Math::Lengthf{0.3_meters}
			)
		, m_suspensionStiffness(typeSerializer.IsValid() ? typeSerializer->ReadWithDefaultValue<float>("suspensionStiffness", 1.5f) : 1.5f)
		, m_suspensionDamping(typeSerializer.IsValid() ? typeSerializer->ReadWithDefaultValue<float>("suspensionDamping", 0.5f) : 0.5f)
		, m_maximumSteeringAngle(
				typeSerializer.IsValid() ? typeSerializer->ReadWithDefaultValue<Math::Anglef>("maxSteeringAngle", 0.3_degrees)
																 : Math::Anglef{0.3_degrees}
			)
		, m_handbrakeMaximumTorque(
				typeSerializer.IsValid() ? typeSerializer->ReadWithDefaultValue<Math::Torquef>("maxHandbrakeTorque", 4000_newtonmeters)
																 : Math::Torquef{4000_newtonmeters}
			)
		, m_forwardGrip(
				typeSerializer.IsValid() ? typeSerializer->ReadWithDefaultValue<Math::Ratiof>("forwardGrip", 100_percent)
																 : Math::Ratiof{100_percent}
			)
		, m_sidewaysGrip(
				typeSerializer.IsValid() ? typeSerializer->ReadWithDefaultValue<Math::Ratiof>("sidewaysGrip", 100_percent)
																 : Math::Ratiof{100_percent}
			)
		, m_defaultRotation(GetRelativeRotation())
	{
	}

	Wheel::Wheel(Initializer&& initializer)
		: BaseType(Forward<Initializer>(initializer))
		, m_defaultRotation(GetRelativeRotation())
	{
	}

	void Wheel::OnCreated()
	{
		if (const Optional<Vehicle*> pVehicle = FindFirstParentOfType<Vehicle>())
		{
			if (const Optional<Axle*> pAxle = FindFirstParentOfType<Axle>())
			{
				pAxle->AddWheel(*this);
			}
			else
			{
				pVehicle->AddWheel(*this);
			}
		}
	}

	void Wheel::OnDestroying()
	{
		if (const Optional<Vehicle*> pVehicle = FindFirstParentOfType<Vehicle>())
		{
			if (const Optional<Axle*> pAxle = FindFirstParentOfType<Axle>())
			{
				pAxle->RemoveWheel(*this);
			}
			else
			{
				pVehicle->RemoveWheel(m_wheelIndex);
			}
			m_wheelIndex = Math::NumericLimits<uint16>::Max;
		}
	}

	void Wheel::OnWorldTransformChanged(const EnumFlags<Entity::TransformChangeFlags> flags)
	{
		if (flags.IsNotSet(Entity::TransformChangeFlags::ChangedByPhysics))
		{
			m_defaultRotation = GetRelativeRotation();
			if (m_wheelIndex != Math::NumericLimits<uint16>::Max)
			{
				if (const Optional<Vehicle*> pVehicle = FindFirstParentOfType<Vehicle>())
				{
					pVehicle->OnWheelTransformChanged(*this);
				}
			}
		}
	}

	[[maybe_unused]] const bool wasWheelRegistered = Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<Wheel>>::Make());
	[[maybe_unused]] const bool wasWheelTypeRegistered = Reflection::Registry::RegisterType<Wheel>();
}
